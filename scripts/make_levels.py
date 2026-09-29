#!/usr/bin/env python3
"""Builds the campaign's maps and level files from room layouts.

Each level is a set of rooms (floor rectangles, each with a wall texture), the
corridors joining them, pillars and accent walls, where the player starts,
which enemies guard which rooms and what supplies lie in them. The script
carves the rooms out of solid wall, gives every wall the texture of the room
it faces, hangs a door in each corridor (locked, for some, with a key found
elsewhere in the level), sets the exit switch in a wall, hides supply rooms
behind push-walls, places enemies, lights and pickups inside their rooms
(deterministically, so rerunning gives the same files) and checks the result:
a solid border, every room reachable from the start, no enemy close to the
start and nothing standing in a wall.

    ./scripts/make_levels.py   # writes assets/maps/*.txt, assets/levels/*.json

Coordinates follow the game's: x is the map row, y the column, and a cell's
centre is at (x + 0.5, y + 0.5).
"""

import json
import math
import random
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"

CONCRETE, BRICK, MOSS, DEMON, EAGLE, EXIT = 1, 2, 3, 4, 5, 6
# How far from the start no enemy may stand, in map units
SAFE_RADIUS = 6.0
# The least distance between two enemies placed in the same room
SPACING = 1.5
# How far from the start and from any enemy a light must stand
LIGHT_CLEARANCE = 1.5
# How far a pickup lies from the start, enemies, lights and other pickups
PICKUP_CLEARANCE = 1.0
# Who walks about their room (the others stand guard, and so do the ones a
# level's objective asks for), how far at most, and how near the start
# their walk may bring them
PATROLLERS = {"soldier", "caco_demon", "shotgun_zombie", "demon"}
PATROL_RADIUS = 5.0
PATROL_CLEARANCE = 4.0


@dataclass
class Room:
    name: str
    x0: int  # floor rectangle, inclusive
    y0: int
    x1: int
    y1: int
    wall: int = CONCRETE
    # (count, type) of enemies in it, or (count, type, "target") for ones
    # a kill_targets objective asks for; and how many lights
    enemies: list = field(default_factory=list)
    lights: int = 0
    light: str = "green_light"
    # (count, type) of pickups lying in it
    pickups: list = field(default_factory=list)
    # Pages of intel pinned to its walls, (title, text): read by walking up
    # to one and looking at it
    intel: list = field(default_factory=list)

    def cells(self):
        for x in range(self.x0, self.x1 + 1):
            for y in range(self.y0, self.y1 + 1):
                yield x, y

    def centre(self):
        return ((self.x0 + self.x1 + 1) / 2, (self.y0 + self.y1 + 1) / 2)


@dataclass
class Level:
    file: str
    name: str
    size: tuple  # rows, columns
    rooms: list
    # Corridors: ((x0, y0), (x1, y1)) straight runs of floor, inclusive
    corridors: list
    start: tuple  # (x, y, theta)
    pillars: list = field(default_factory=list)  # (x, y, texture)
    # Wall cells given a feature texture (banner, demon faces): (x, y, texture)
    accents: list = field(default_factory=list)
    # Corridors (by index) whose door is locked: "gold" or "silver". The key
    # is a pickup in some room, and must be reachable without its own door.
    locks: dict = field(default_factory=dict)
    # The exit switch: a wall cell next to a room's floor
    exit: tuple = None
    # What the level asks before its exit opens: {"type", "text"}
    objectives: list = field(default_factory=list)
    # Shown before the level starts, and under its results once cleared
    briefing: str = ""
    debrief: str = ""
    # Its track, assets/music/<music>.mp3; the level file's name if none
    music: str = ""
    # How many hidden supply rooms, each behind a push-wall
    secrets: int = 0
    seed: int = 1


def carve(level):
    rows, cols = level.size
    grid = [[None] * cols for _ in range(rows)]  # None: wall, else room index
    for index, room in enumerate(level.rooms):
        for x, y in room.cells():
            grid[x][y] = index
    for (x0, y0), (x1, y1) in level.corridors:
        for x in range(min(x0, x1), max(x0, x1) + 1):
            for y in range(min(y0, y1), max(y0, y1) + 1):
                if grid[x][y] is None:
                    grid[x][y] = -1  # corridor
    for x, y, _ in level.pillars:
        grid[x][y] = None
    return grid


SECRET_DEPTH = 3  # cells of hidden room behind a push-wall
SECRET_SUPPLIES = ("large_medkit", "ammo_box")


def secrets(level, grid):
    """Carves level.secrets hidden rooms, each behind a push-wall in a
    straight stretch of a room's wall with solid rock behind it. Returns
    (x, y, dx, dy) of each push-wall and the pickups inside."""
    rows, cols = level.size
    rng = random.Random(level.seed + 7)  # apart from the other placements
    rooms = [room for index, room in enumerate(level.rooms) if index > 0]
    rng.shuffle(rooms)
    found, supplies = [], []
    for room in rooms:
        if len(found) == level.secrets:
            break
        spots = [(x, y, dx, dy) for x, y in room.cells()
                 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
        rng.shuffle(spots)
        for fx, fy, dx, dy in spots:
            wx, wy = fx + dx, fy + dy
            lx, ly = dy, dx  # across the way in
            # Solid: the wall, its neighbours along the wall, the room behind
            # and a margin round it (which may be the map's border); the room
            # itself inside the border
            def solid(k, side):
                x, y = wx + k * dx + side * lx, wy + k * dy + side * ly
                inner = 1 <= k <= SECRET_DEPTH and abs(side) <= 1
                if inner:
                    inside = 0 < x < rows - 1 and 0 < y < cols - 1
                else:
                    inside = 0 <= x < rows and 0 <= y < cols
                return inside and grid[x][y] is None
            if not all(solid(k, side) for k in range(0, SECRET_DEPTH + 2)
                       for side in range(-2, 3)):
                continue
            if (wx, wy) == level.exit or any(
                    (wx, wy) == (x, y) for x, y, _ in level.accents):
                continue
            for k in range(1, SECRET_DEPTH + 1):
                for side in (-1, 0, 1):
                    grid[wx + k * dx + side * lx][wy + k * dy + side * ly] = -2
            found.append((wx, wy, dx, dy))
            # Either side of where the wall ends up, two cells in
            for side, kind in zip((-1, 1), SECRET_SUPPLIES):
                x = wx + 2 * dx + side * lx
                y = wy + 2 * dy + side * ly
                supplies.append({"type": kind,
                                 "position": {"x": x + 0.5, "y": y + 0.5}})
            break
    if len(found) != level.secrets:
        raise ValueError(f"{level.file}: room for only {len(found)} of "
                         f"{level.secrets} secrets")
    return found, supplies


def texture_map(level, grid):
    """Each wall takes the texture of the room it faces (or the nearest)."""
    rows, cols = level.size
    cells = [[0] * cols for _ in range(rows)]
    for x in range(rows):
        for y in range(cols):
            if grid[x][y] is not None:
                continue
            best, best_distance = CONCRETE, math.inf
            for dx in range(-3, 4):
                for dy in range(-3, 4):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < rows and 0 <= ny < cols:
                        owner = grid[nx][ny]
                        if owner is not None and owner >= 0:
                            distance = abs(dx) + abs(dy)
                            if distance < best_distance:
                                best = level.rooms[owner].wall
                                best_distance = distance
            cells[x][y] = best
    for x, y, texture in level.pillars + level.accents:
        cells[x][y] = texture
    if level.exit is not None:
        cells[level.exit[0]][level.exit[1]] = EXIT
    return cells


def check(level, cells):
    rows, cols = level.size
    for x in range(rows):
        for y in range(cols):
            if (x in (0, rows - 1) or y in (0, cols - 1)) and cells[x][y] == 0:
                raise ValueError(f"{level.file}: open border at {x},{y}")
    sx, sy = int(level.start[0]), int(level.start[1])
    if cells[sx][sy] != 0:
        raise ValueError(f"{level.file}: the start is in a wall")
    # Everything open must be reachable from the start
    seen = {(sx, sy)}
    queue = deque([(sx, sy)])
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if cells[nx][ny] == 0 and (nx, ny) not in seen:
                seen.add((nx, ny))
                queue.append((nx, ny))
    for room in level.rooms:
        if not any(cell in seen for cell in room.cells()):
            raise ValueError(f"{level.file}: room {room.name} is unreachable")
    return seen


def patrol_radius(room, post, start):
    """How far an enemy posted at `post` walks about: over most of its room
    (it only goes where it sees from its post, so never out of it), but not
    near the start. 0, too little to walk: it stands guard."""
    longest = max(room.x1 - room.x0, room.y1 - room.y0) + 1
    radius = min(PATROL_RADIUS, longest / 2 - 1,
                 math.dist(post, start) - PATROL_CLEARANCE)
    return round(radius, 1) if radius >= 1.5 else 0.0


def place(level, cells, reachable):
    """Enemies away from walls, lights and pickups along them."""
    rng = random.Random(level.seed)
    start = (level.start[0], level.start[1])
    enemies, lights, pickups = [], [], []

    def open_around(x, y):
        return all(cells[x + dx][y + dy] == 0
                   for dx in (-1, 0, 1) for dy in (-1, 0, 1))

    for room in level.rooms:
        interior = [(x, y) for x, y in room.cells()
                    if (x, y) in reachable and open_around(x, y)]
        rng.shuffle(interior)
        placed = []
        for count, kind, *mark in room.enemies:
            for _ in range(count):
                for x, y in interior:
                    point = (x + 0.5, y + 0.5)
                    if math.dist(point, start) < SAFE_RADIUS:
                        continue
                    if any(math.dist(point, other) < SPACING for other in placed):
                        continue
                    placed.append(point)
                    # Facing the room's centre, as if keeping watch over it
                    cx, cy = room.centre()
                    theta = round(math.atan2(cy - point[1], cx - point[0]), 2)
                    enemy = {"type": kind,
                             "position": {"x": point[0], "y": point[1],
                                          "theta": theta}}
                    if mark == ["target"]:
                        enemy["target"] = True
                    elif kind in PATROLLERS:
                        radius = patrol_radius(room, point, start)
                        if radius > 0:
                            enemy["patrol_radius"] = radius
                    enemies.append(enemy)
                    break
                else:
                    raise ValueError(f"{level.file}: no room for a {kind} "
                                     f"in {room.name}")
        # Lights along the walls: open cells next to a wall
        edge = [(x, y) for x, y in room.cells()
                if cells[x][y] == 0 and any(
                    cells[x + dx][y + dy] != 0
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
        rng.shuffle(edge)
        placed_lights = 0
        for x, y in edge:
            if placed_lights == room.lights:
                break
            point = (x + 0.5, y + 0.5)
            if math.dist(point, start) < LIGHT_CLEARANCE or any(
                    math.dist(point, other) < LIGHT_CLEARANCE
                    for other in placed):
                continue
            lights.append({"type": room.light,
                           "position": {"x": point[0], "y": point[1]}})
            placed_lights += 1
        # Supplies stacked against the walls, clear of everything else
        taken = placed + [(light["position"]["x"], light["position"]["y"])
                          for light in lights]
        spots = edge + [cell for cell in interior if cell not in edge]
        for count, kind in room.pickups:
            for _ in range(count):
                for x, y in spots:
                    point = (x + 0.5, y + 0.5)
                    if (math.dist(point, start) < PICKUP_CLEARANCE or
                            any(math.dist(point, other) < PICKUP_CLEARANCE
                                for other in taken)):
                        continue
                    taken.append(point)
                    pickups.append({"type": kind,
                                    "position": {"x": point[0],
                                                 "y": point[1]}})
                    break
                else:
                    raise ValueError(f"{level.file}: no room for a {kind} "
                                     f"in {room.name}")
    return enemies, lights, pickups


def pin_intel(level, cells, reachable, lights, hidden):
    """Each room's pages of intel on its walls: a face in a straight run of
    wall (not a pillar, a doorway's side, the exit, a secret or a banner),
    read from an open cell of the room with no light standing in it."""
    rng = random.Random(level.seed + 13)  # apart from the other placements
    lit = {(int(light["position"]["x"]), int(light["position"]["y"]))
           for light in lights}
    special = ({(x, y) for x, y, _, _ in hidden} |
               {(x, y) for x, y, _ in level.accents})
    pinned = []
    for room in level.rooms:
        faces = []
        for fx, fy in room.cells():
            if cells[fx][fy] != 0 or (fx, fy) not in reachable or (fx, fy) in lit:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                wx, wy = fx + dx, fy + dy
                along = [(wx + dy, wy + dx), (wx - dy, wy - dx)]
                if (cells[wx][wy] in (0, EXIT) or (wx, wy) in special or
                        any(cells[x][y] == 0 for x, y in along)):
                    continue
                if any((p["x"], p["y"]) == (wx, wy) for p in pinned):
                    continue
                faces.append((wx, wy, -dx, -dy))
        rng.shuffle(faces)
        for title, text in room.intel:
            if not faces:
                raise ValueError(f"{level.file}: no wall for intel in "
                                 f"{room.name}")
            wx, wy, dx, dy = faces.pop()
            faces = [face for face in faces if face[:2] != (wx, wy)]
            pinned.append({"x": wx, "y": wy, "dx": dx, "dy": dy,
                           "title": title, "text": text})
    return pinned


def doors(level, cells):
    """A door in each corridor: its middle cell with walls facing each other
    across it and the way through open on the other two sides."""
    placed = []
    for index, ((x0, y0), (x1, y1)) in enumerate(level.corridors):
        run = [(x, y) for x in range(min(x0, x1), max(x0, x1) + 1)
               for y in range(min(y0, y1), max(y0, y1) + 1)]
        candidates = []
        for x, y in run:
            walls_y = cells[x][y - 1] != 0 and cells[x][y + 1] != 0
            open_x = cells[x - 1][y] == 0 and cells[x + 1][y] == 0
            walls_x = cells[x - 1][y] != 0 and cells[x + 1][y] != 0
            open_y = cells[x][y - 1] == 0 and cells[x][y + 1] == 0
            if (walls_y and open_x) or (walls_x and open_y):
                candidates.append((x, y))
        lock = level.locks.get(index)
        if candidates:
            placed.append((candidates[len(candidates) // 2], lock))
        elif lock is not None:
            raise ValueError(f"{level.file}: corridor {index} is locked but "
                             f"has no cell a door fits in")
    return placed


def write(level):
    grid = carve(level)
    hidden, hidden_supplies = secrets(level, grid)
    cells = texture_map(level, grid)
    reachable = check(level, cells)
    enemies, lights, pickups = place(level, cells, reachable)
    pickups += hidden_supplies
    intel = pin_intel(level, cells, reachable, lights, hidden)
    door_marks = {cell: {"gold": "G", "silver": "S", None: "D"}[lock]
                  for cell, lock in doors(level, cells)}
    door_cells = door_marks.keys()
    rows, cols = level.size
    map_file = level.file.replace(".json", ".txt")
    lines = [f"height {rows}", f"width {cols}"]
    lines += ["".join(door_marks.get((x, y), "X" if c == EXIT else str(c))
                      for y, c in enumerate(row))
              for x, row in enumerate(cells)]
    (ASSETS / "maps" / map_file).write_text("\n".join(lines) + "\n")
    data = {
        "name": level.name,
        "briefing": level.briefing,
        "debrief": level.debrief,
        # Its track, assets/music/<name>.mp3 (scripts/import_freedoom.py)
        "music": level.music or level.file.removesuffix(".json"),
        "map": map_file,
        "player": {"position": {"x": level.start[0], "y": level.start[1],
                                "theta": level.start[2]}},
        "enemies": enemies,
        "dynamicObjects": lights,
        "pickups": pickups,
        "objectives": level.objectives,
        "secrets": [{"x": x, "y": y, "dx": dx, "dy": dy}
                    for x, y, dx, dy in hidden],
        "intel": intel,
        "staticObjects": [],
    }
    (ASSETS / "levels" / level.file).write_text(json.dumps(data, indent=2) + "\n")
    kinds = {}
    for enemy in enemies:
        kinds[enemy["type"]] = kinds.get(enemy["type"], 0) + 1
    supplies = {}
    for pickup in pickups:
        supplies[pickup["type"]] = supplies.get(pickup["type"], 0) + 1
    locked = sum(mark != "D" for mark in door_marks.values())
    print(f"{level.file}: {rows}x{cols}, {kinds}, {len(lights)} lights, "
          f"{len(door_cells)} doors ({locked} locked), {len(hidden)} secrets, "
          f"{len(intel)} intel, {supplies}")


EAST, SOUTH, WEST, NORTH = 1.57, 0.0, -1.57, 3.14


def row_of(x, y0, y1, texture):
    """Pillars along row x from column y0 to y1: a train, a row of crates."""
    return [(x, y, texture) for y in range(y0, y1 + 1)]


LEVELS = [
    # A concrete checkpoint: guard hall, barracks, a yard and two back rooms.
    # Soldiers only.
    Level("level1.json", "CHECKPOINT", (22, 30), seed=11,
          start=(2.5, 3.5, EAST),
          rooms=[
              Room("gatehouse", 1, 1, 4, 6, lights=2, pickups=[(1, "ammo_box")]),
              Room("guard hall", 1, 9, 6, 16, enemies=[(2, "soldier")],
                   lights=2),
              Room("barracks", 1, 19, 8, 24, BRICK,
                   enemies=[(2, "soldier")], lights=2, light="red_light",
                   intel=[("A LETTER HOME", "Dear Mother, they have moved the whole company up to the castle except for us twelve, and nobody says why. At night the trucks go up the road with their lamps off, and the mountain hums. I sleep badly. The sergeant says it is the generators. Your loving son, Paul.")]),
              Room("yard", 9, 2, 14, 23, enemies=[(1, "soldier")], lights=4,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("armory", 17, 1, 20, 8, enemies=[(1, "soldier")], lights=1,
                   pickups=[(2, "ammo_box"), (1, "gold_key"), (1, "mp5")]),
              Room("office", 17, 12, 20, 24, BRICK,
                   enemies=[(1, "soldier")], lights=2, light="red_light",
                   pickups=[(1, "medkit")],
                   intel=[("STANDING ORDERS", "No vehicle goes up the road without a Directorate pass. No vehicle comes down it at all. Anyone coming down from the castle on foot is to be turned back, by force if need be, and is not to be spoken to. For the Directorate: Reiss, Commandant.")]),
          ],
          corridors=[((3, 7), (3, 8)), ((4, 17), (4, 18)), ((5, 3), (8, 3)),
                     ((7, 12), (8, 12)), ((9, 21), (9, 21)),
                     ((15, 4), (16, 4)), ((15, 18), (16, 18))],
          pillars=[(11, 7, CONCRETE), (11, 12, CONCRETE), (11, 17, CONCRETE),
                   (12, 7, CONCRETE), (12, 12, CONCRETE), (12, 17, CONCRETE)],
          accents=[(0, 21, EAGLE), (21, 18, EAGLE)],
          locks={6: "gold"},  # the office
          exit=(21, 23),
          secrets=1,
          briefing="A checkpoint guards the only road into the valley. Its garrison is small but alert. The way on is through the officer's office, and the gold key to it is kept in the armory. Clear the checkpoint and get out through the office.",          debrief="The checkpoint's radio log copies the castle's numbers hour by hour. They are not the same every hour, as the reports said: the last digit falls by one each night.",

          objectives=[{"type": "kill_all",
                       "text": "Clear the checkpoint of its guards"}]),

    # The valley's supply depot: sheds round a yard of parked trucks. The
    # first shotgunners, and the shotgun.
    Level("depot.json", "THE DEPOT", (24, 36), seed=88,
          start=(2.5, 2.5, EAST), music="level2",
          rooms=[
              Room("gate", 1, 1, 4, 6, lights=2, pickups=[(1, "ammo_box")]),
              Room("guard hut", 1, 9, 5, 15, enemies=[(1, "soldier")],
                   lights=2, pickups=[(1, "medkit")]),
              Room("warehouse", 1, 18, 5, 30, BRICK,
                   enemies=[(2, "soldier")], lights=3, light="red_light",
                   pickups=[(1, "ammo_box"), (1, "shotgun")],
                   intel=[("A FIELD PACK", "The second team's, left in a locker as if its owner meant to come back for it. A camera with no film. A map of the valley with the castle ringed twice. A note in pencil: Hale says we go in by the railway. Nobody comes out by the road.")]),
              Room("yard", 8, 1, 15, 22,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie")], lights=4,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("fuel store", 8, 25, 15, 30,
                   enemies=[(1, "soldier")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("garage", 18, 1, 22, 10,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=2,
                   pickups=[(1, "gold_key"), (1, "medkit")]),
              Room("quartermaster", 18, 14, 22, 30, BRICK,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=2,
                   light="red_light",
                   pickups=[(1, "ammo_box"), (1, "large_medkit")],
                   intel=[("A MANIFEST", "Night convoy 41. Twelve crates, marked: fragile, keep cold, do not open. For the works, lower gate. Escort of six. The quartermaster adds: double the escort. The drivers of convoy 39 never came back, and their truck came in by itself, in the right gear, with its lamps off.")]),
          ],
          corridors=[((2, 7), (2, 8)), ((5, 3), (7, 3)), ((3, 16), (3, 17)),
                     ((6, 12), (7, 12)), ((6, 27), (7, 27)),
                     ((11, 23), (11, 24)), ((16, 5), (17, 5)),
                     ((16, 18), (17, 18))],
          pillars=[(10, 5, BRICK), (10, 6, BRICK), (10, 11, BRICK),
                   (10, 12, BRICK), (13, 8, BRICK), (13, 9, BRICK),
                   (13, 15, BRICK), (13, 16, BRICK), (10, 18, BRICK),
                   (10, 19, BRICK)],
          accents=[(0, 23, EAGLE), (0, 25, EAGLE)],
          locks={7: "gold"},  # the quartermaster's
          exit=(23, 28),
          secrets=1,
          briefing="The checkpoint's trucks were supplied from a depot down the road. Its sheds are still guarded, and some of the guards are no longer quite soldiers. The gate out is in the quartermaster's office, locked with a gold key; the drivers kept theirs in the garage.",
          debrief="Every truck in the depot went one way: up. Not one crate ever came back down the valley. The last convoy is still out somewhere, on the road to the barracks.",
          objectives=[{"type": "kill_all",
                       "text": "Clear the depot of its guards"}]),

    # Red-brick barracks around a pillared courtyard. The first demons.
    Level("level2.json", "THE BARRACKS", (26, 30), seed=22,
          start=(23.5, 2.5, NORTH),
          rooms=[
              Room("entrance", 21, 1, 24, 6, BRICK, lights=2,
                   light="red_light", pickups=[(1, "ammo_box")]),
              Room("mess hall", 13, 1, 18, 9, BRICK,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=2,
                   light="red_light",
                   pickups=[(1, "medkit")]),
              Room("courtyard", 8, 11, 17, 20, CONCRETE,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=4,
                   pickups=[(1, "ammo_box")]),
              Room("dormitory", 1, 1, 9, 8, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light", pickups=[(1, "medkit"), (1, "chainsaw")]),
              Room("chapel", 1, 12, 5, 27, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=3,
                   light="red_light", pickups=[(1, "gold_key")],
                   intel=[("THE CHAPLAIN'S DIARY", "Tuesday. The men will not come to the chapel since the floor began to crack. I told them it was the frost. Tonight I put my ear to the stones and heard something under them knocking, patient, as if it knew that someone would come and open the door. God forgive me, I almost did.")]),
              Room("quarters", 20, 12, 24, 27, BRICK,
                   enemies=[(1, "shotgun_zombie")], lights=2, light="red_light",
                   pickups=[(1, "medkit")],
                   intel=[("SICK LIST", "Week three: fourteen men sick. Cold hands, no appetite, no sleep. Four men reported dead at reveille have since returned to duty. The medical officer declines to explain this, and has asked to be sent to the front.")]),
              Room("store", 8, 23, 17, 28, CONCRETE, lights=2,
                   pickups=[(2, "ammo_box"), (1, "large_medkit")]),
          ],
          corridors=[((19, 3), (20, 3)), ((10, 4), (12, 4)),
                     ((15, 10), (15, 10)), ((6, 15), (7, 15)),
                     ((18, 16), (19, 16)), ((12, 21), (12, 22)),
                     ((6, 25), (7, 25)), ((18, 25), (19, 25))],
          pillars=[(10, 13, BRICK), (10, 18, BRICK), (15, 13, BRICK),
                   (15, 18, BRICK)],
          accents=[(0, 19, EAGLE), (0, 20, EAGLE), (25, 19, EAGLE)],
          locks={1: "gold"},  # the dormitory
          exit=(25, 26),
          secrets=1,
          briefing="The last convoy got as far as the barracks. Something has come up from under the chapel there: the soldiers are not alone any more. Wipe out the garrison and take the stairs at the back of the quarters.",          debrief="The chapel floor was broken open from below. What came up did not come to fight the garrison. It came for them, and the ones it took got up again.",

          objectives=[{"type": "kill_all",
                       "text": "Wipe out the barracks garrison"}]),

    # A relay station on the ridge: a mast yard, operators' hall, generators
    # and the transmitter room. The super shotgun.
    Level("radio_station.json", "THE RELAY", (28, 34), seed=99,
          start=(24.5, 2.5, NORTH), music="level1",
          rooms=[
              Room("road", 22, 1, 26, 6, lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("gatehouse", 22, 9, 26, 16,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=2,
                   pickups=[(1, "medkit")]),
              Room("mast yard", 10, 1, 19, 16,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=4,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("generator room", 1, 1, 7, 8, BRICK,
                   enemies=[(1, "shotgun_zombie"), (1, "demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "super_shotgun"), (1, "ammo_box")]),
              Room("operators' hall", 1, 11, 7, 20,
                   enemies=[(2, "soldier")], lights=2,
                   pickups=[(1, "silver_key"), (1, "ammo_box")],
                   intel=[("THE OPERATORS' ORDERS", "Repeat the castle's numbers on every band, every hour, without a break. Do not answer any station that answers. Do not write down anything that comes back.")]),
              Room("dormitory", 10, 19, 16, 28, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("transmitter room", 1, 23, 7, 28, DEMON,
                   enemies=[(1, "caco_demon"), (1, "shotgun_zombie")],
                   lights=2, light="red_light",
                   pickups=[(1, "gold_key"), (1, "large_medkit")],
                   intel=[("WHAT CAME BACK", "I wrote it down anyway. Every hour, after we send the castle's eleven digits, eleven more come in on the dead band. Ours go down by one each night. Theirs go up. The officer says that when the two are the same, the gate opens. I asked him from which side. He did not answer.")]),
              Room("cable stair", 19, 19, 26, 28,
                   enemies=[(1, "demon"), (1, "soldier")], lights=2,
                   pickups=[(1, "ammo_box")]),
          ],
          corridors=[((24, 7), (24, 8)), ((20, 3), (21, 3)),
                     ((20, 12), (21, 12)), ((8, 4), (9, 4)),
                     ((8, 14), (9, 14)), ((4, 9), (4, 10)),
                     ((4, 21), (4, 22)), ((13, 17), (13, 18)),
                     ((17, 24), (18, 24))],
          pillars=[(13, 6, CONCRETE), (13, 11, CONCRETE), (16, 6, CONCRETE),
                   (16, 11, CONCRETE)],
          accents=[(0, 14, EAGLE), (0, 17, EAGLE)],
          # the transmitter room; the stair down
          locks={6: "silver", 8: "gold"},
          exit=(27, 26),
          secrets=1,
          briefing="On the ridge above the barracks stands the relay that repeats the castle's numbers to the world. Its operators have not left their posts in three weeks. Silence the relay and take the cable stair down; the transmitter room's gold key opens it.",
          debrief="The relay's cable does not run out of the valley. It runs down, into the rock under the barracks, towards the old catacombs.",
          objectives=[{"type": "kill_all",
                       "text": "Silence the relay: kill its operators"}]),

    # Catacombs of mossy stone: narrow tunnels between burial chambers, and
    # demons that rush along them.
    Level("level3.json", "THE CATACOMBS", (28, 30), seed=33,
          start=(1.5, 1.5, SOUTH),
          rooms=[
              Room("stair", 1, 1, 4, 4, MOSS, lights=1,
                   pickups=[(1, "ammo_box")]),
              Room("ossuary", 1, 9, 6, 17, MOSS,
                   enemies=[(1, "soldier"), (1, "demon")], lights=2,
                   pickups=[(1, "medkit"), (1, "silver_key")],
                   intel=[("BURIAL REGISTER", "The castle chapel's register of burials. The last entry is two hundred years old. Under it, in fresh ink and a shaking hand: They are not staying buried. We are moving them lower.")]),
              Room("crypt", 9, 1, 15, 7, MOSS,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   pickups=[(2, "ammo_box")]),
              Room("hall of faces", 9, 11, 16, 20, DEMON,
                   enemies=[(1, "shotgun_zombie"), (2, "caco_demon")], lights=3,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("well", 1, 22, 9, 28, MOSS,
                   enemies=[(1, "caco_demon")], lights=2,
                   pickups=[(1, "large_medkit"), (1, "gold_key")]),
              Room("tomb", 19, 3, 26, 12, MOSS,
                   enemies=[(1, "demon"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "medkit"), (1, "ammo_box")],
                   intel=[("HALE'S NOTEBOOK: DAY 4", "Lost Moreau and Okafor in the tunnels. The things down here do not bleed like men. The cart track runs down to the works, and the works go up to the castle. If I am right, it is all one machine. Day 5: going down.")]),
              Room("altar", 19, 16, 26, 28, DEMON,
                   enemies=[(1, "cyber_demon", "target")], lights=3,
                   light="red_light",
                   pickups=[(1, "ammo_box")]),
          ],
          corridors=[((2, 5), (2, 8)), ((5, 2), (8, 2)), ((7, 13), (8, 13)),
                     ((12, 8), (12, 10)), ((4, 18), (4, 21)),
                     ((10, 21), (10, 25)), ((16, 5), (18, 5)),
                     ((17, 18), (18, 18)), ((23, 13), (23, 15))],
          pillars=[(12, 14, DEMON), (12, 17, DEMON), (13, 14, DEMON),
                   (13, 17, DEMON), (22, 21, MOSS), (22, 23, MOSS)],
          accents=[(27, 21, DEMON), (27, 22, DEMON), (8, 16, DEMON)],
          # the tomb; both ways into the altar
          locks={6: "silver", 7: "gold", 8: "gold"},
          exit=(27, 26),
          secrets=2,
          briefing="The relay's cable leads into the old catacombs under the barracks, and on to a burial hall where a cyber demon keeps watch over an altar. The way down is sealed with two locks. Find the keys, destroy the demon and throw the switch behind the altar.",          debrief="Behind the altar a cart track runs down into the rock, worn smooth by heavy loads. Crates of red stone lie spilled along it, still warm to the touch.",

          objectives=[{"type": "kill_targets",
                       "text": "Destroy the cyber demon at the altar"}]),

    # The mine: drifts and galleries in mossy rock, and the seam of red
    # stone at the bottom, with its overseer.
    Level("mine.json", "THE MINE", (30, 34), seed=111,
          start=(2.5, 2.5, SOUTH), music="level3",
          rooms=[
              Room("cage", 1, 1, 4, 5, MOSS, lights=1,
                   pickups=[(1, "ammo_box")]),
              Room("sorting hall", 1, 8, 7, 19,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie")], lights=3,
                   pickups=[(1, "medkit")]),
              Room("north drift", 1, 22, 4, 32, MOSS,
                   enemies=[(1, "demon")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("gallery", 10, 1, 18, 10, MOSS,
                   enemies=[(1, "shotgun_zombie"), (1, "demon"),
                            (1, "caco_demon")], lights=3,
                   pickups=[(1, "silver_key"), (1, "medkit")],
                   intel=[("A SHIFT REPORT", "Third shift. Forty blocks of red stone cut from the new face. The stone is warm when it comes out, and colder every hour after. Two men put their hands on it to warm them and would not take them off again. They were sent up to the castle with the blocks.")]),
              Room("stope", 10, 13, 16, 22, MOSS,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("cart depot", 7, 25, 14, 32,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie")], lights=2,
                   pickups=[(1, "ammo_box"), (1, "large_medkit")]),
              Room("seam", 21, 1, 28, 14, DEMON,
                   enemies=[(1, "minigun_zombie", "target"), (1, "demon")],
                   lights=3, light="red_light",
                   pickups=[(1, "gold_key"), (1, "ammo_box")],
                   intel=[("HALE'S NOTEBOOK: DAY 6", "The mine goes deeper than any map of it. At the bottom a seam of the red stone runs through the rock like a vein, and it beats, slowly, about once an hour. I timed it against my watch. It beats in time with the numbers.")]),
              Room("cart track", 19, 18, 28, 32, MOSS,
                   enemies=[(1, "caco_demon"), (1, "soldier"),
                            (1, "shotgun_zombie")], lights=3,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
          ],
          corridors=[((2, 6), (2, 7)), ((5, 3), (9, 3)), ((2, 20), (2, 21)),
                     ((8, 15), (9, 15)), ((13, 11), (13, 12)),
                     ((5, 28), (6, 28)), ((12, 23), (12, 24)),
                     ((19, 5), (20, 5)), ((15, 28), (18, 28))],
          pillars=[(12, 4, MOSS), (12, 7, MOSS), (16, 4, MOSS), (16, 7, MOSS),
                   (22, 22, MOSS), (22, 28, MOSS), (25, 22, MOSS),
                   (25, 28, MOSS)],
          accents=[(29, 6, DEMON), (29, 9, DEMON)],
          # the seam; the cart track
          locks={7: "silver", 8: "gold"},
          exit=(29, 30),
          secrets=1,
          briefing="The cart track behind the altar runs down into a mine. Its overseer has the gold key to the cart track that climbs to the forge, and keeps to the seam at the bottom, behind a silver lock. Find the silver key in the gallery, go down and kill the overseer.",
          debrief="At the bottom of the mine the seam runs on, deeper than any lamp reaches, beating slowly in time with the numbers. The cart track climbs from here to the forge.",
          objectives=[{"type": "kill_targets",
                       "text": "Kill the overseer at the seam"}]),

    # The forge: a works of concrete and brick round a pillared foundry.
    Level("level4.json", "THE FORGE", (28, 36), seed=55,
          start=(24.5, 2.5, NORTH),
          rooms=[
              Room("loading dock", 22, 1, 26, 8, lights=2,
                   pickups=[(1, "ammo_box"), (1, "medkit")]),
              Room("yard", 14, 1, 19, 12,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("foundry", 1, 1, 11, 14, BRICK,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie"),
                            (1, "caco_demon")], lights=4, light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("furnace", 1, 17, 8, 30, BRICK,
                   enemies=[(1, "demon"), (2, "caco_demon")], lights=3,
                   light="red_light",
                   pickups=[(1, "silver_key"), (1, "large_medkit")],
                   intel=[("THE FOREMAN'S ORDERS", "Blocks are cut to the drawings and to nothing else. A block that cracks is not thrown away: it goes back down the mine it came from. No talking near the furnaces after dark. The men say the stone listens. I say that is enough talk.")]),
              Room("workshop", 12, 16, 20, 30,
                   enemies=[(1, "soldier"), (1, "demon"), (1, "caco_demon")],
                   lights=3,
                   pickups=[(2, "ammo_box"), (1, "medkit"),
                            (1, "rocket_launcher")]),
              Room("store room", 23, 12, 26, 20, enemies=[(1, "soldier")],
                   lights=1, pickups=[(1, "ammo_box"), (1, "medkit")]),
              Room("office", 23, 24, 26, 30, BRICK, enemies=[(1, "soldier")],
                   lights=1, light="red_light", pickups=[(1, "ammo_box")]),
          ],
          corridors=[((20, 4), (21, 4)), ((12, 6), (13, 6)),
                     ((4, 15), (4, 16)), ((9, 24), (11, 24)),
                     ((17, 13), (17, 15)), ((24, 9), (24, 11)),
                     ((25, 21), (25, 23))],
          pillars=[(4, 4, CONCRETE), (4, 8, CONCRETE), (4, 12, CONCRETE),
                   (8, 4, CONCRETE), (8, 8, CONCRETE), (8, 12, CONCRETE)],
          accents=[(0, 6, EAGLE), (0, 9, EAGLE)],
          locks={6: "silver"},  # the office
          exit=(27, 28),
          secrets=1,
          briefing="At the top of the cart track from the mine the enemy has built a forge, and what it makes is going up to the sanctum. The foreman keeps the silver key to his office by the furnaces. Kill every hand in the works and leave through the office.",          debrief="The forge's order book is exact: so many tons of red stone, cut into so many blocks, shipped up to the sanctum by night. The book calls the blocks gate stones.",

          objectives=[{"type": "kill_all",
                       "text": "Shut the forge down: kill every guard"}]),

    # The laboratory: white halls, a specimen room, surgery and the test
    # chamber where the lens was ground. The plasma rifle.
    Level("laboratory.json", "THE LABORATORY", (30, 32), seed=122,
          start=(27.5, 15.5, NORTH), music="level5",
          rooms=[
              Room("airlock", 26, 13, 28, 18, lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("hall", 20, 4, 23, 27,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie")], lights=4,
                   pickups=[(1, "medkit")]),
              Room("specimen room", 12, 1, 17, 9, MOSS,
                   enemies=[(1, "demon"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "ammo_box"), (1, "silver_key")]),
              Room("test chamber", 10, 12, 17, 20, DEMON,
                   enemies=[(1, "minigun_zombie"), (1, "caco_demon")],
                   lights=3, light="red_light",
                   pickups=[(1, "large_medkit"), (1, "plasma_rifle")]),
              Room("surgery", 12, 23, 17, 30,
                   enemies=[(1, "shotgun_zombie"), (1, "demon")], lights=2,
                   pickups=[(1, "medkit"), (1, "ammo_box")],
                   intel=[("DR. MAREN'S NOTES", "The stone is not a mineral. Cut thin, it is a lens. Through it the room behind looks the same, but something stands in it that is not in the room. It is always standing a little nearer than the last time you looked.")]),
              Room("cold store", 1, 1, 8, 9, MOSS,
                   enemies=[(1, "caco_demon"), (1, "soldier")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("Maren's office", 1, 12, 7, 20, BRICK,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=2,
                   light="red_light",
                   pickups=[(1, "gold_key"), (1, "medkit")],
                   intel=[("DR. MAREN'S LAST NOTE", "The Directorate wants a ring of the stone, twenty metres across, in the sanctum. I have told them that a lens that size will not show us the other side. It will show the other side us. They said that was the point.")]),
              Room("observation", 1, 23, 8, 30, DEMON,
                   enemies=[(1, "minigun_zombie"), (1, "demon"),
                            (1, "caco_demon")], lights=3,
                   light="red_light",
                   pickups=[(1, "ammo_box"), (1, "medkit")]),
          ],
          corridors=[((24, 15), (25, 15)), ((18, 6), (19, 6)),
                     ((18, 16), (19, 16)), ((18, 26), (19, 26)),
                     ((9, 5), (11, 5)), ((4, 10), (4, 11)),
                     ((8, 16), (9, 16)), ((9, 26), (11, 26))],
          pillars=[(12, 14, DEMON), (12, 18, DEMON), (15, 14, DEMON),
                   (15, 18, DEMON)],
          accents=[(29, 14, EAGLE), (29, 17, EAGLE)],
          # the test chamber, both ways in; the observation room
          locks={2: "silver", 6: "silver", 7: "gold"},
          exit=(0, 27),
          secrets=1,
          briefing="The forge's blocks were shaped to drawings from the laboratory, where Dr. Maren studied the red stone. The way on is through the observation room, gold-locked; her office holds the key. Something was left in the test chamber, behind a silver lock. Clear the laboratory.",
          debrief="Dr. Maren's office is empty and her last pages are torn out. Scratched into her desk: the archivist knows the date. Ask the archivist.",
          objectives=[{"type": "kill_all",
                       "text": "Clear the laboratory"}]),

    # The archives: brick stacks round a pillared reading hall, and a vault.
    Level("level5.json", "THE ARCHIVES", (30, 30), seed=66,
          start=(27.5, 14.5, NORTH),
          rooms=[
              Room("lobby", 25, 12, 28, 17, lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("reading hall", 16, 8, 22, 21, BRICK,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie")], lights=4,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")],
                   intel=[("A MEMO", "To all departments. From tonight the archives will keep the count and nothing else. All other records are to be burned. The Directorate thanks you for your years of careful work, which will not now be needed.")]),
              Room("west stacks", 16, 1, 26, 5, BRICK,
                   enemies=[(2, "soldier")], lights=2,
                   pickups=[(1, "silver_key"), (1, "ammo_box")]),
              Room("east stacks", 16, 24, 26, 28, BRICK,
                   enemies=[(1, "demon"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "medkit")]),
              Room("catalogue", 8, 1, 13, 9, enemies=[(1, "soldier"),
                                                      (1, "caco_demon")],
                   lights=2, pickups=[(1, "ammo_box")]),
              Room("map room", 8, 20, 13, 28, BRICK,
                   enemies=[(1, "soldier"), (1, "minigun_zombie")], lights=2,
                   light="red_light",
                   pickups=[(1, "gold_key"), (1, "ammo_box")]),
              Room("records", 1, 1, 6, 7, MOSS, enemies=[(1, "caco_demon")],
                   lights=1, pickups=[(1, "large_medkit")],
                   intel=[("A PERSONNEL FILE", "Voss, E., archivist. Moved into the vault at his own request forty days ago, and has not come out since. Meals left at the door are not taken. The door is warm to the touch. Staff are not to knock.")]),
              Room("vault", 1, 10, 6, 19, DEMON,
                   enemies=[(1, "cyber_demon", "target"), (1, "soldier")],
                   lights=3, light="red_light",
                   pickups=[(1, "ammo_box"), (1, "medkit")]),
          ],
          corridors=[((23, 14), (24, 14)), ((19, 6), (19, 7)),
                     ((19, 22), (19, 23)), ((25, 6), (25, 11)),
                     ((14, 3), (15, 3)), ((14, 26), (15, 26)),
                     ((7, 4), (7, 4)), ((3, 8), (3, 9)),
                     ((14, 9), (15, 9))],
          pillars=[(18, 11, BRICK), (18, 18, BRICK), (20, 11, BRICK),
                   (20, 18, BRICK)],
          accents=[(29, 13, EAGLE), (29, 16, EAGLE), (0, 12, EAGLE),
                   (0, 17, EAGLE)],
          locks={5: "silver", 7: "gold"},  # the map room; the vault
          exit=(0, 15),
          secrets=1,
          briefing="Dr. Maren's note points to the archives, where the orders for the forge and the laboratory were written. A cyber demon they call the archivist sits in the vault among the records. The vault takes a gold key from the map room, and the map room a silver one from the stacks. Destroy the archivist.",          debrief="In the archivist's vault, the ledger that explains the numbers: a count of the nights until the gate is finished. By the ledger, three are left.",

          objectives=[{"type": "kill_targets",
                       "text": "Destroy the archivist in the vault"}]),

    # The rail yard: trains standing on the tracks, sheds round them, and
    # the last train up the mountain.
    Level("rail_yard.json", "THE RAIL YARD", (30, 40), seed=133,
          start=(27.5, 2.5, EAST), music="level4",
          rooms=[
              Room("platform", 26, 1, 28, 10, lights=2,
                   pickups=[(1, "ammo_box"), (1, "medkit")]),
              Room("signal box", 20, 1, 23, 8, BRICK,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=1,
                   pickups=[(1, "silver_key")],
                   intel=[("A TIMETABLE", "Up trains at 23:00, 01:00 and 03:00: gate stones and personnel. Down trains: none. From tonight no down trains will run at all. Anyone who wishes to leave the castle may apply to the Directorate in writing.")]),
              Room("yard", 8, 1, 17, 34,
                   enemies=[(3, "soldier"), (2, "shotgun_zombie"),
                            (1, "minigun_zombie"), (1, "caco_demon")],
                   lights=6, pickups=[(1, "medkit"), (2, "ammo_box")]),
              Room("engine shed", 20, 12, 27, 22, BRICK,
                   enemies=[(1, "demon"), (1, "caco_demon"), (1, "soldier")],
                   lights=2, light="red_light",
                   pickups=[(1, "ammo_box"), (1, "large_medkit")],
                   intel=[("HALE'S NOTEBOOK: DAY 9", "Rode up on a stone train, under a tarpaulin. The guards on the wall do not sleep, do not eat and never look down the mountain. I think the Directorate came up here to build a door, and has become its lock. Going in over the wall.")]),
              Room("coal store", 1, 1, 5, 12,
                   enemies=[(1, "demon"), (1, "shotgun_zombie")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("workshop", 1, 15, 5, 24, BRICK,
                   enemies=[(1, "minigun_zombie"), (1, "soldier")], lights=2,
                   light="red_light",
                   pickups=[(1, "gold_key"), (1, "medkit")]),
              Room("loco hall", 1, 27, 5, 34,
                   enemies=[(1, "caco_demon"), (1, "shotgun_zombie")],
                   lights=2, pickups=[(1, "ammo_box"), (1, "medkit")]),
              Room("last train", 20, 26, 28, 34, DEMON,
                   enemies=[(1, "cyber_demon", "target"), (1, "caco_demon")],
                   lights=3, light="red_light",
                   pickups=[(1, "ammo_box"), (1, "large_medkit")]),
          ],
          corridors=[((24, 4), (25, 4)), ((27, 11), (27, 11)),
                     ((18, 5), (19, 5)), ((18, 17), (19, 17)),
                     ((6, 6), (7, 6)), ((6, 20), (7, 20)),
                     ((6, 30), (7, 30)), ((18, 30), (19, 30))],
          pillars=(row_of(10, 3, 10, CONCRETE) + row_of(10, 24, 31, CONCRETE)
                   + row_of(13, 12, 22, CONCRETE)
                   + row_of(15, 3, 9, CONCRETE)
                   + row_of(15, 25, 31, CONCRETE)),
          accents=[(29, 29, EAGLE), (29, 31, EAGLE)],
          # the workshop; the last train
          locks={5: "silver", 7: "gold"},
          exit=(29, 32),
          secrets=1,
          briefing="The railway up the mountain starts in this yard. The last train stands loaded with the final gate stone, and a cyber demon guards it behind a gold lock. The gold key is in the workshop, the workshop's silver key in the signal box. Stop the train.",
          debrief="The last train will not go up tonight: its stone lies cracked across the tracks. You will go up instead, the way Hale went, by the wall.",
          objectives=[{"type": "kill_targets",
                       "text": "Destroy the cyber demon at the last train"}]),

    # The outer wall: wall walks, towers at the corners, a barbican and the
    # gate hall, where the way goes down.
    Level("outer_wall.json", "THE OUTER WALL", (30, 34), seed=144,
          start=(27.5, 16.5, NORTH), music="level6",
          rooms=[
              Room("postern", 26, 14, 28, 19, MOSS, lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("south walk", 20, 1, 23, 32, MOSS,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie"),
                            (1, "minigun_zombie")], lights=4,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("west walk", 11, 1, 17, 5, MOSS,
                   enemies=[(1, "soldier"), (1, "demon")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("east walk", 11, 28, 17, 32, MOSS,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "medkit")]),
              Room("west tower", 1, 1, 8, 7,
                   enemies=[(1, "minigun_zombie"), (1, "soldier")], lights=2,
                   pickups=[(1, "silver_key"), (1, "ammo_box")]),
              Room("east tower", 1, 26, 8, 32,
                   enemies=[(1, "caco_demon"), (1, "shotgun_zombie")],
                   lights=2, pickups=[(1, "medkit"), (1, "ammo_box")],
                   intel=[("SENTRY ORDERS", "Sentries face inward. Nothing is expected to come up the mountain. Watch the courtyard, watch the keep and above all watch the sanctum doors. If they open before the count is done, fire on whatever comes out. Then on yourselves.")]),
              Room("barbican", 11, 9, 17, 24,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie"),
                            (1, "caco_demon")], lights=4,
                   pickups=[(1, "large_medkit"), (1, "ammo_box")]),
              Room("gate hall", 1, 11, 8, 22, DEMON,
                   enemies=[(1, "minigun_zombie"), (1, "caco_demon"),
                            (1, "shotgun_zombie")], lights=3,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
          ],
          corridors=[((24, 16), (25, 16)), ((18, 3), (19, 3)),
                     ((18, 30), (19, 30)), ((18, 16), (19, 16)),
                     ((9, 3), (10, 3)), ((9, 30), (10, 30)),
                     ((9, 16), (10, 16)), ((14, 6), (14, 8)),
                     ((14, 25), (14, 27))],
          pillars=[(13, 12, MOSS), (13, 21, MOSS), (15, 12, MOSS),
                   (15, 21, MOSS), (4, 14, DEMON), (4, 19, DEMON)],
          accents=[(29, 15, EAGLE), (29, 18, EAGLE), (0, 14, DEMON),
                   (0, 19, DEMON)],
          locks={6: "silver"},  # the gate hall
          exit=(0, 16),
          secrets=1,
          briefing="Castle Wolfenstein. Its outer wall is manned by sentries who watch the castle, not the mountain. The way in and down is through the gate hall, silver-locked; the west tower holds the key. Take the wall.",
          debrief="From the wall you can see the sanctum's roof and the red light under it. Scratched by the gate hall's stair, an arrow and a name: HALE. It points down, into the dungeons.",
          objectives=[{"type": "kill_all",
                       "text": "Take the outer wall"}]),

    # The dungeons: a cell block, a guard room, an interrogation room, the
    # oubliette and the drain the way out goes through.
    Level("dungeons.json", "THE DUNGEONS", (30, 32), seed=155,
          start=(2.5, 15.5, SOUTH), music="level3",
          rooms=[
              Room("stair foot", 1, 13, 4, 18, MOSS, lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("guard room", 1, 1, 6, 9,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")], lights=2,
                   pickups=[(1, "silver_key"), (1, "ammo_box")]),
              Room("interrogation", 1, 22, 5, 30, BRICK,
                   enemies=[(1, "soldier"), (1, "minigun_zombie")], lights=2,
                   light="red_light", pickups=[(1, "medkit")],
                   intel=[("AN INTERROGATION", "The prisoner will not give his name or his unit. He asks only what the date is, and when he is told, he laughs. He says we are counting the wrong way.")]),
              Room("cell block", 7, 12, 24, 19, MOSS,
                   enemies=[(2, "shotgun_zombie"), (1, "demon"),
                            (1, "soldier")], lights=4,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("cell 1", 8, 7, 10, 9, MOSS, enemies=[(1, "demon")]),
              Room("cell 2", 13, 7, 15, 9, MOSS,
                   pickups=[(1, "ammo_box")]),
              Room("cell 3", 18, 7, 20, 9, MOSS,
                   enemies=[(1, "shotgun_zombie")]),
              Room("cell 4", 8, 22, 10, 24, MOSS,
                   enemies=[(1, "caco_demon")]),
              Room("cell 5", 13, 22, 15, 24, MOSS,
                   pickups=[(1, "medkit")]),
              Room("Hale's cell", 18, 22, 20, 24, MOSS,
                   pickups=[(1, "large_medkit")],
                   intel=[("SCRATCHED ON A CELL WALL", "Tally marks, hundreds of them, then: HALE. ELEVEN DAYS. THEY TAKE ONE OF US EACH NIGHT TO THE SANCTUM. THEY DO NOT COME BACK DEAD. THE BELL TOWER SENDS THE NUMBERS. STOP THE BELL AND THE COUNT STOPS. I COULD NOT.")]),
              Room("oubliette", 23, 1, 28, 9, DEMON,
                   enemies=[(1, "demon"), (1, "caco_demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "gold_key"), (1, "ammo_box")]),
              Room("drain", 26, 12, 28, 28, MOSS,
                   enemies=[(1, "caco_demon"), (1, "demon"),
                            (1, "shotgun_zombie")], lights=3,
                   pickups=[(1, "medkit")]),
          ],
          corridors=[((2, 10), (2, 12)), ((2, 19), (2, 21)),
                     ((5, 15), (6, 15)),
                     ((9, 10), (9, 11)), ((14, 10), (14, 11)),
                     ((19, 10), (19, 11)), ((9, 20), (9, 21)),
                     ((14, 20), (14, 21)), ((19, 20), (19, 21)),
                     ((6, 23), (7, 23)), ((21, 8), (22, 8)),
                     ((25, 15), (25, 15))],
          accents=[(29, 20, DEMON), (29, 24, DEMON)],
          # Hale's cell; the drain
          locks={8: "silver", 11: "gold"},
          exit=(29, 26),
          secrets=1,
          briefing="Under the wall lie the castle's dungeons, where the second team was taken. Look for what is left of Hale. The drain out of the dungeons is gold-locked; the key went down the oubliette. Clear the cells.",
          debrief="Hale's cell is empty and the count on its wall stops at eleven days. What is left of his work is yours now: the bell tower, and the transmitter in it.",
          objectives=[{"type": "kill_all",
                       "text": "Clear the dungeons"}]),

    # The keep: a castle round a pillared courtyard, towers at its corners
    # and a great hall behind gold-locked doors.
    Level("level6.json", "THE KEEP", (30, 32), seed=77,
          start=(27.5, 15.5, NORTH),
          rooms=[
              Room("gate", 25, 13, 28, 18, lights=2,
                   pickups=[(1, "ammo_box"), (1, "medkit")]),
              Room("courtyard", 13, 8, 22, 23, MOSS,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie"),
                            (1, "caco_demon")], lights=4,
                   pickups=[(1, "ammo_box")]),
              Room("west tower", 13, 1, 22, 5,
                   enemies=[(1, "soldier"), (1, "demon")],
                   lights=2, pickups=[(1, "medkit")]),
              Room("east tower", 13, 26, 22, 30,
                   enemies=[(1, "soldier"), (1, "shotgun_zombie")],
                   lights=2, pickups=[(1, "ammo_box")]),
              Room("guardroom", 25, 1, 28, 9,
                   enemies=[(1, "soldier"), (1, "minigun_zombie")],
                   lights=1, pickups=[(1, "silver_key"), (1, "ammo_box")],
                   intel=[("THE COMMANDANT'S LAST ORDER", "To all who remain. The count will end on time. Those still able to choose may leave by the north stair. Those who cannot choose any more will hold the keep. I will be in the bell tower, keeping the numbers going to the end. Reiss.")]),
              Room("chapel", 1, 1, 9, 5, MOSS, enemies=[(1, "caco_demon")],
                   lights=2, light="red_light",
                   pickups=[(1, "large_medkit")]),
              Room("armory", 1, 26, 9, 30,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "gold_key"), (2, "ammo_box")]),
              Room("great hall", 1, 8, 9, 23, DEMON,
                   enemies=[(1, "cyber_demon"), (1, "soldier"),
                            (1, "demon"), (1, "caco_demon")], lights=4,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
          ],
          corridors=[((23, 15), (24, 15)), ((17, 6), (17, 7)),
                     ((17, 24), (17, 25)), ((10, 15), (12, 15)),
                     ((10, 3), (12, 3)), ((10, 28), (12, 28)),
                     ((26, 10), (26, 12)), ((23, 3), (24, 3)),
                     ((5, 6), (5, 7)), ((5, 24), (5, 25))],
          pillars=[(15, 11, MOSS), (15, 20, MOSS), (20, 11, MOSS),
                   (20, 20, MOSS), (3, 12, DEMON), (3, 19, DEMON),
                   (7, 12, DEMON), (7, 19, DEMON)],
          accents=[(29, 14, EAGLE), (29, 17, EAGLE), (0, 10, DEMON),
                   (0, 20, DEMON)],
          # the great hall, every way in; the armory
          locks={3: "gold", 8: "gold", 9: "gold", 5: "silver"},
          exit=(0, 15),
          secrets=2,
          briefing="The keep stands between the dungeons and the bell tower. Its great hall is shut with gold locks, and the gold key is in the armory behind a silver one; the guards at the gate carry that. Take the keep and go through the great hall.",          debrief="The keep's garrison was never guarding the castle from the valley. It was guarding the valley from what the sanctum lets through.",

          objectives=[{"type": "kill_all",
                       "text": "Take the keep: kill its garrison"}]),

    # The bell tower: its base, two stairs, the clock room, the ringing
    # chamber and the belfry, where the transmitter is.
    Level("bell_tower.json", "THE BELL TOWER", (30, 30), seed=166,
          start=(27.5, 14.5, NORTH), music="level5",
          rooms=[
              Room("tower door", 26, 12, 28, 17, BRICK, lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("base hall", 18, 6, 23, 23, BRICK,
                   enemies=[(2, "soldier"), (1, "shotgun_zombie"),
                            (1, "caco_demon")], lights=4,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("west stair", 8, 1, 21, 3, BRICK,
                   enemies=[(1, "demon")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("east stair", 8, 26, 21, 28, BRICK,
                   enemies=[(1, "shotgun_zombie")], lights=2,
                   pickups=[(1, "medkit")]),
              Room("clock room", 9, 7, 15, 22, DEMON,
                   enemies=[(1, "minigun_zombie"), (1, "caco_demon"),
                            (1, "demon")], lights=3, light="red_light",
                   pickups=[(1, "silver_key"), (1, "ammo_box")]),
              Room("ringing chamber", 1, 1, 5, 12, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "gold_key"), (1, "large_medkit")]),
              Room("belfry", 1, 17, 5, 28, DEMON,
                   enemies=[(1, "minigun_zombie", "target"),
                            (1, "cyber_demon")], lights=3,
                   light="red_light",
                   pickups=[(1, "ammo_box"), (1, "medkit")],
                   intel=[("THE TRANSMITTER LOG", "The numbers are not ours. The transmitter only repeats what the stone tells it, and the stone counts down by itself. We were never building the gate. We were only keeping it company while it built itself. Reiss.")]),
          ],
          corridors=[((24, 14), (25, 14)), ((20, 4), (20, 5)),
                     ((20, 24), (20, 25)), ((10, 4), (10, 6)),
                     ((10, 23), (10, 25)), ((6, 2), (7, 2)),
                     ((6, 20), (8, 20)), ((6, 27), (7, 27))],
          pillars=[(11, 11, DEMON), (11, 18, DEMON), (13, 11, DEMON),
                   (13, 18, DEMON), (20, 10, BRICK), (20, 19, BRICK)],
          accents=[(29, 13, EAGLE), (29, 16, EAGLE)],
          # the ringing chamber; both ways into the belfry
          locks={5: "silver", 6: "gold", 7: "gold"},
          exit=(0, 22),
          secrets=1,
          briefing="The numbers go out from the belfry of the bell tower, and Commandant Reiss keeps them going. The belfry is gold-locked; the ringing chamber holds the key, behind a silver lock opened from the clock room. Kill the commandant and stop the count.",
          debrief="The transmitter is wrecked and the bell has fallen through the floor. For a moment the valley is silent. Then, from under the castle, the count goes on without it. One night is left.",
          objectives=[{"type": "kill_targets",
                       "text": "Kill Commandant Reiss in the belfry"}]),

    # The sanctum: banner halls leading to an arena where it ends.
    Level("level7.json", "THE SANCTUM", (30, 32), seed=44,
          start=(28.5, 15.5, NORTH),
          rooms=[
              Room("vestibule", 25, 12, 28, 19, BRICK, lights=2,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("west wing", 16, 1, 23, 9, BRICK,
                   enemies=[(1, "shotgun_zombie"), (1, "minigun_zombie"),
                            (1, "caco_demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box"),
                            (1, "silver_key")]),
              Room("east wing", 16, 22, 23, 30, BRICK,
                   enemies=[(1, "soldier"), (1, "minigun_zombie"),
                            (1, "caco_demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("nave", 13, 11, 22, 20, DEMON,
                   enemies=[(1, "demon"), (1, "caco_demon"),
                            (1, "cyber_demon")], lights=4,
                   light="red_light", pickups=[(1, "large_medkit")]),
              Room("cloister", 5, 1, 12, 8, MOSS,
                   enemies=[(1, "caco_demon"), (1, "demon")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("reliquary", 5, 23, 12, 30, MOSS,
                   enemies=[(1, "caco_demon"), (1, "cyber_demon")], lights=2,
                   pickups=[(1, "medkit"), (1, "ammo_box"), (1, "gold_key")],
                   intel=[("A PAGE IN ANOTHER HAND", "Eleven digits, the castle's own numbers, written in a hand that is not quite a hand. Under them one word, in the Directorate's cipher, which the clerks who broke it did not believe: WELCOME.")]),
              Room("arena", 1, 10, 10, 21, DEMON,
                   enemies=[(2, "cyber_demon", "target")], lights=4,
                   light="red_light",
                   pickups=[(1, "large_medkit"), (1, "ammo_box")]),
          ],
          corridors=[((23, 15), (24, 16)), ((19, 10), (19, 10)),
                     ((19, 21), (19, 21)), ((13, 4), (15, 4)),
                     ((13, 27), (15, 27)), ((11, 15), (12, 15)),
                     ((8, 9), (8, 9)), ((8, 22), (8, 22))],
          pillars=[(15, 13, DEMON), (15, 18, DEMON), (20, 13, DEMON),
                   (20, 18, DEMON), (4, 13, DEMON), (4, 18, DEMON),
                   (7, 13, DEMON), (7, 18, DEMON)],
          accents=[(0, 14, EAGLE), (0, 17, EAGLE), (29, 14, EAGLE),
                   (29, 17, EAGLE)],
          # the east wing; every way into the arena
          locks={2: "silver", 5: "gold", 6: "gold", 7: "gold"},
          exit=(0, 15),
          secrets=2,
          briefing="The sanctum is where it began. Two cyber demons guard the arena at its heart, behind gold-locked gates. Take the silver key from the west wing, the gold one from the reliquary, and end this.",          debrief="The ring of gate stones is cold. Whatever was coming through will have to find another door.",

          objectives=[{"type": "kill_targets",
                       "text": "Kill the cyber demons guarding the arena"}]),
]


def main():
    for level in LEVELS:
        write(level)


if __name__ == "__main__":
    main()
