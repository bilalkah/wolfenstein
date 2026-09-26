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


@dataclass
class Room:
    name: str
    x0: int  # floor rectangle, inclusive
    y0: int
    x1: int
    y1: int
    wall: int = CONCRETE
    # (count, type) of enemies guarding it, or (count, type, "target") for
    # ones a kill_targets objective asks for; and how many lights
    enemies: list = field(default_factory=list)
    lights: int = 0
    light: str = "green_light"
    # (count, type) of pickups lying in it
    pickups: list = field(default_factory=list)

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
    # Shown before the level starts
    briefing: str = ""
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
        "map": map_file,
        "player": {"position": {"x": level.start[0], "y": level.start[1],
                                "theta": level.start[2]}},
        "enemies": enemies,
        "dynamicObjects": lights,
        "pickups": pickups,
        "objectives": level.objectives,
        "secrets": [{"x": x, "y": y, "dx": dx, "dy": dy}
                    for x, y, dx, dy in hidden],
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
          f"{supplies}")


EAST, SOUTH, WEST, NORTH = 1.57, 0.0, -1.57, 3.14

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
                   enemies=[(2, "soldier")], lights=2, light="red_light"),
              Room("yard", 9, 2, 14, 23, enemies=[(1, "soldier")], lights=4,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("armory", 17, 1, 20, 8, enemies=[(1, "soldier")], lights=1,
                   pickups=[(2, "ammo_box"), (1, "gold_key"), (1, "mp5")]),
              Room("office", 17, 12, 20, 24, BRICK,
                   enemies=[(1, "soldier")], lights=2, light="red_light",
                   pickups=[(1, "medkit")]),
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
          briefing="A checkpoint guards the only road into the valley. Its garrison is small but alert. The way on is through the officer's office, and the gold key to it is kept in the armory. Clear the checkpoint and get out through the office.",
          objectives=[{"type": "kill_all",
                       "text": "Clear the checkpoint of its guards"}]),

    # Red-brick barracks around a pillared courtyard. The first demons.
    Level("level2.json", "THE BARRACKS", (26, 30), seed=22,
          start=(23.5, 2.5, NORTH),
          rooms=[
              Room("entrance", 21, 1, 24, 6, BRICK, lights=2,
                   light="red_light", pickups=[(1, "ammo_box")]),
              Room("mess hall", 13, 1, 18, 9, BRICK,
                   enemies=[(2, "soldier")], lights=2, light="red_light",
                   pickups=[(1, "medkit")]),
              Room("courtyard", 8, 11, 17, 20, CONCRETE,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=4,
                   pickups=[(1, "ammo_box")]),
              Room("dormitory", 1, 1, 9, 8, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light", pickups=[(1, "medkit")]),
              Room("chapel", 1, 12, 5, 27, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=3,
                   light="red_light", pickups=[(1, "gold_key")]),
              Room("quarters", 20, 12, 24, 27, BRICK,
                   enemies=[(1, "soldier")], lights=2, light="red_light",
                   pickups=[(1, "medkit")]),
              Room("store", 8, 23, 17, 28, CONCRETE, lights=2,
                   pickups=[(2, "ammo_box"), (1, "large_medkit"),
                            (1, "shotgun")]),
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
          briefing="Past the checkpoint lie the barracks. Something has come up from under the chapel: the soldiers are not alone any more. Wipe out the garrison and take the stairs at the back of the quarters.",
          objectives=[{"type": "kill_all",
                       "text": "Wipe out the barracks garrison"}]),

    # Catacombs of mossy stone: narrow tunnels between burial chambers.
    Level("level3.json", "THE CATACOMBS", (28, 30), seed=33,
          start=(1.5, 1.5, SOUTH),
          rooms=[
              Room("stair", 1, 1, 4, 4, MOSS, lights=1,
                   pickups=[(1, "ammo_box")]),
              Room("ossuary", 1, 9, 6, 17, MOSS,
                   enemies=[(2, "soldier")], lights=2,
                   pickups=[(1, "medkit"), (1, "silver_key")]),
              Room("crypt", 9, 1, 15, 7, MOSS,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("hall of faces", 9, 11, 16, 20, DEMON,
                   enemies=[(1, "soldier"), (2, "caco_demon")], lights=3,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("well", 1, 22, 9, 28, MOSS,
                   enemies=[(1, "caco_demon")], lights=2,
                   pickups=[(1, "large_medkit"), (1, "gold_key")]),
              Room("tomb", 19, 3, 26, 12, MOSS,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
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
          briefing="Under the barracks the old catacombs open into a burial hall where a cyber demon keeps watch over an altar. The way down is sealed with two locks. Find the keys, destroy the demon and throw the switch behind the altar.",
          objectives=[{"type": "kill_targets",
                       "text": "Destroy the cyber demon at the altar"}]),

    # The sanctum: banner halls leading to an arena where it ends.
    Level("level4.json", "THE SANCTUM", (30, 32), seed=44,
          start=(28.5, 15.5, NORTH),
          rooms=[
              Room("vestibule", 25, 12, 28, 19, BRICK, lights=2,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("west wing", 16, 1, 23, 9, BRICK,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box"),
                            (1, "silver_key")]),
              Room("east wing", 16, 22, 23, 30, BRICK,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light",
                   pickups=[(1, "medkit"), (1, "ammo_box")]),
              Room("nave", 13, 11, 22, 20, DEMON,
                   enemies=[(1, "soldier"), (1, "caco_demon"),
                            (1, "cyber_demon")], lights=4,
                   light="red_light", pickups=[(1, "large_medkit")]),
              Room("cloister", 5, 1, 12, 8, MOSS,
                   enemies=[(1, "caco_demon")], lights=2,
                   pickups=[(1, "ammo_box")]),
              Room("reliquary", 5, 23, 12, 30, MOSS,
                   enemies=[(1, "caco_demon"), (1, "cyber_demon")], lights=2,
                   pickups=[(1, "medkit"), (1, "ammo_box"), (1, "gold_key")]),
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
          briefing="The sanctum is where it began. Two cyber demons guard the arena at its heart, behind gold-locked gates. Take the silver key from the west wing, the gold one from the reliquary, and end this.",
          objectives=[{"type": "kill_targets",
                       "text": "Kill the cyber demons guarding the arena"}]),
]


def main():
    for level in LEVELS:
        write(level)


if __name__ == "__main__":
    main()
