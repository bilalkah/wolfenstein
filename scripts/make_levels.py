#!/usr/bin/env python3
"""Builds the campaign's maps and level files from room layouts.

Each level is a set of rooms (floor rectangles, each with a wall texture), the
corridors joining them, pillars and accent walls, where the player starts and
which enemies guard which rooms. The script carves the rooms out of solid
wall, gives every wall the texture of the room it faces, places enemies and
lights inside their rooms (deterministically, so rerunning gives the same
files) and checks the result: a solid border, every room reachable from the
start, no enemy close to the start and nothing standing in a wall.

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

CONCRETE, BRICK, MOSS, DEMON, EAGLE = 1, 2, 3, 4, 5
# How far from the start no enemy may stand, in map units
SAFE_RADIUS = 6.0
# The least distance between two enemies placed in the same room
SPACING = 1.5
# How far from the start and from any enemy a light must stand
LIGHT_CLEARANCE = 1.5


@dataclass
class Room:
    name: str
    x0: int  # floor rectangle, inclusive
    y0: int
    x1: int
    y1: int
    wall: int = CONCRETE
    # (count, type) of enemies guarding it, and how many lights
    enemies: list = field(default_factory=list)
    lights: int = 0
    light: str = "green_light"

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
    """Enemies and lights on open cells of their rooms, away from walls."""
    rng = random.Random(level.seed)
    start = (level.start[0], level.start[1])
    enemies, lights = [], []

    def open_around(x, y):
        return all(cells[x + dx][y + dy] == 0
                   for dx in (-1, 0, 1) for dy in (-1, 0, 1))

    for room in level.rooms:
        interior = [(x, y) for x, y in room.cells()
                    if (x, y) in reachable and open_around(x, y)]
        rng.shuffle(interior)
        placed = []
        for count, kind in room.enemies:
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
                    enemies.append({"type": kind,
                                    "position": {"x": point[0], "y": point[1],
                                                 "theta": theta}})
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
    return enemies, lights


def write(level):
    grid = carve(level)
    cells = texture_map(level, grid)
    reachable = check(level, cells)
    enemies, lights = place(level, cells, reachable)
    rows, cols = level.size
    map_file = level.file.replace(".json", ".txt")
    lines = [f"height {rows}", f"width {cols}"]
    lines += ["".join(str(c) for c in row) for row in cells]
    (ASSETS / "maps" / map_file).write_text("\n".join(lines) + "\n")
    data = {
        "name": level.name,
        "map": map_file,
        "player": {"position": {"x": level.start[0], "y": level.start[1],
                                "theta": level.start[2]}},
        "enemies": enemies,
        "dynamicObjects": lights,
        "staticObjects": [],
    }
    (ASSETS / "levels" / level.file).write_text(json.dumps(data, indent=2) + "\n")
    kinds = {}
    for enemy in enemies:
        kinds[enemy["type"]] = kinds.get(enemy["type"], 0) + 1
    print(f"{level.file}: {rows}x{cols}, {kinds}, {len(lights)} lights")


EAST, SOUTH, WEST, NORTH = 1.57, 0.0, -1.57, 3.14

LEVELS = [
    # A concrete checkpoint: guard hall, barracks, a yard and two back rooms.
    # Soldiers only.
    Level("level1.json", "CHECKPOINT", (22, 26), seed=11,
          start=(2.5, 3.5, EAST),
          rooms=[
              Room("gatehouse", 1, 1, 4, 6, lights=2),
              Room("guard hall", 1, 9, 6, 16, enemies=[(2, "soldier")],
                   lights=2),
              Room("barracks", 1, 19, 8, 24, BRICK,
                   enemies=[(2, "soldier")], lights=2, light="red_light"),
              Room("yard", 9, 2, 14, 23, enemies=[(1, "soldier")], lights=4),
              Room("armory", 17, 1, 20, 8, enemies=[(1, "soldier")], lights=1),
              Room("office", 17, 12, 20, 24, BRICK,
                   enemies=[(1, "soldier")], lights=2, light="red_light"),
          ],
          corridors=[((3, 7), (3, 8)), ((4, 17), (4, 18)), ((5, 3), (8, 3)),
                     ((7, 12), (8, 12)), ((9, 21), (9, 21)),
                     ((15, 4), (16, 4)), ((15, 18), (16, 18))],
          pillars=[(11, 7, CONCRETE), (11, 12, CONCRETE), (11, 17, CONCRETE),
                   (12, 7, CONCRETE), (12, 12, CONCRETE), (12, 17, CONCRETE)],
          accents=[(0, 21, EAGLE), (21, 18, EAGLE)]),

    # Red-brick barracks around a pillared courtyard. The first demons.
    Level("level2.json", "THE BARRACKS", (26, 30), seed=22,
          start=(23.5, 2.5, NORTH),
          rooms=[
              Room("entrance", 21, 1, 24, 6, BRICK, lights=2,
                   light="red_light"),
              Room("mess hall", 13, 1, 18, 9, BRICK,
                   enemies=[(2, "soldier")], lights=2, light="red_light"),
              Room("courtyard", 8, 11, 17, 20, CONCRETE,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=4),
              Room("dormitory", 1, 1, 9, 8, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light"),
              Room("chapel", 1, 12, 5, 27, BRICK,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=3,
                   light="red_light"),
              Room("quarters", 20, 12, 24, 27, BRICK,
                   enemies=[(1, "soldier")], lights=2, light="red_light"),
              Room("store", 8, 23, 17, 28, CONCRETE, lights=2),
          ],
          corridors=[((19, 3), (20, 3)), ((10, 4), (12, 4)),
                     ((15, 10), (15, 10)), ((6, 15), (7, 15)),
                     ((18, 16), (19, 16)), ((12, 21), (12, 22)),
                     ((6, 25), (7, 25)), ((18, 25), (19, 25))],
          pillars=[(10, 13, BRICK), (10, 18, BRICK), (15, 13, BRICK),
                   (15, 18, BRICK)],
          accents=[(0, 19, EAGLE), (0, 20, EAGLE), (25, 19, EAGLE)]),

    # Catacombs of mossy stone: narrow tunnels between burial chambers.
    Level("level3.json", "THE CATACOMBS", (28, 30), seed=33,
          start=(1.5, 1.5, SOUTH),
          rooms=[
              Room("stair", 1, 1, 4, 4, MOSS, lights=1),
              Room("ossuary", 1, 9, 6, 17, MOSS,
                   enemies=[(2, "soldier")], lights=2),
              Room("crypt", 9, 1, 15, 7, MOSS,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2),
              Room("hall of faces", 9, 11, 16, 20, DEMON,
                   enemies=[(1, "soldier"), (2, "caco_demon")], lights=3,
                   light="red_light"),
              Room("well", 1, 22, 9, 28, MOSS,
                   enemies=[(1, "caco_demon")], lights=2),
              Room("tomb", 19, 3, 26, 12, MOSS,
                   enemies=[(1, "soldier"), (1, "caco_demon")], lights=2),
              Room("altar", 19, 16, 26, 28, DEMON,
                   enemies=[(1, "cyber_demon")], lights=3, light="red_light"),
          ],
          corridors=[((2, 5), (2, 8)), ((5, 2), (8, 2)), ((7, 13), (8, 13)),
                     ((12, 8), (12, 10)), ((4, 18), (4, 21)),
                     ((10, 21), (10, 25)), ((16, 5), (18, 5)),
                     ((17, 18), (18, 18)), ((23, 13), (23, 15))],
          pillars=[(12, 14, DEMON), (12, 17, DEMON), (13, 14, DEMON),
                   (13, 17, DEMON), (22, 21, MOSS), (22, 23, MOSS)],
          accents=[(27, 21, DEMON), (27, 22, DEMON), (8, 16, DEMON)]),

    # The sanctum: banner halls leading to an arena where it ends.
    Level("level4.json", "THE SANCTUM", (30, 32), seed=44,
          start=(28.5, 15.5, NORTH),
          rooms=[
              Room("vestibule", 25, 12, 28, 19, BRICK, lights=2,
                   light="red_light"),
              Room("west wing", 16, 1, 23, 9, BRICK,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light"),
              Room("east wing", 16, 22, 23, 30, BRICK,
                   enemies=[(2, "soldier"), (1, "caco_demon")], lights=2,
                   light="red_light"),
              Room("nave", 13, 11, 22, 20, DEMON,
                   enemies=[(1, "soldier"), (1, "caco_demon"),
                            (1, "cyber_demon")], lights=4,
                   light="red_light"),
              Room("cloister", 5, 1, 12, 8, MOSS,
                   enemies=[(1, "caco_demon")], lights=2),
              Room("reliquary", 5, 23, 12, 30, MOSS,
                   enemies=[(1, "caco_demon"), (1, "cyber_demon")], lights=2),
              Room("arena", 1, 10, 10, 21, DEMON,
                   enemies=[(2, "cyber_demon")], lights=4,
                   light="red_light"),
          ],
          corridors=[((23, 15), (24, 16)), ((19, 10), (19, 10)),
                     ((19, 21), (19, 21)), ((13, 4), (15, 4)),
                     ((13, 27), (15, 27)), ((11, 15), (12, 16)),
                     ((8, 9), (8, 9)), ((8, 22), (8, 22))],
          pillars=[(15, 13, DEMON), (15, 18, DEMON), (20, 13, DEMON),
                   (20, 18, DEMON), (4, 13, DEMON), (4, 18, DEMON),
                   (7, 13, DEMON), (7, 18, DEMON)],
          accents=[(0, 14, EAGLE), (0, 17, EAGLE), (29, 14, EAGLE),
                   (29, 17, EAGLE)]),
]


def main():
    for level in LEVELS:
        write(level)


if __name__ == "__main__":
    main()
