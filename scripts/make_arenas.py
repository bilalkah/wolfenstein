#!/usr/bin/env python3
"""Builds the multiplayer arenas' maps and level files from drawings.

Each arena is drawn cell by cell, one text row a map row: walls of a few
textures, pillars, doors, the floor, where players come in and what lies
about. The script writes the map (assets/maps/<name>.txt) and the level
(assets/levels/<name>.json), and checks the drawing first: a solid border,
every open cell reachable from every other, doors hung in walls, and spawn
points on open floor with nothing a body bumps into beside them and apart
from each other.

    ./scripts/make_arenas.py   # writes the arenas named in ARENAS

Coordinates follow the game's: x is the map row, y the column, and a cell's
centre is at (x + 0.5, y + 0.5).

The drawings' legend:

    #  wall (the arena's main texture)     .  floor
    %  wall (its second texture)           D  door
    &  wall (a third, for accents)         o  pillar (a wall cell)
    S  floor where a player comes in, facing the arena's middle
    m  medkit      M  large medkit      a  ammo box
    g  shotgun     G  super shotgun     u  mp5 ("smg")
    c  chainsaw    r  rocket launcher   p  plasma rifle
    l  a green light (a lamp players walk round)
"""

import json
import math
from collections import deque
from dataclasses import dataclass
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"

CONCRETE, BRICK, MOSS, DEMON, EAGLE = 1, 2, 3, 4, 5
# The least distance between two spawn points
SPAWN_SPACING = 4.0
PICKUPS = {
    "m": "medkit",
    "M": "large_medkit",
    "a": "ammo_box",
    "g": "shotgun",
    "G": "super_shotgun",
    "u": "mp5",
    "c": "chainsaw",
    "r": "rocket_launcher",
    "p": "plasma_rifle",
}
FLOOR = set(".SDl") | set(PICKUPS)


@dataclass
class Arena:
    name: str  # the files' name
    title: str  # shown as the level's name
    music: str
    walls: tuple  # the textures of '#', '%', '&' and 'o'
    drawing: str


# A market town at noon: two long streets, west and east, behind the
# houses, and the market between them, a hall, its stalls and a stall block
# in the middle, crossed by walls with gaps in them; doors between the
# streets and the market on every block, and an alley along the south.
# Inspired by the desert maps players know, drawn afresh.
BAZAAR = Arena(
    name="bazaar",
    title="THE BAZAAR",
    music="level3",
    walls=(BRICK, CONCRETE, EAGLE, CONCRETE),
    drawing="""
####################################
#S......#..................#......S#
#.......#...u....S.....a...#.......#
#...o..mD..................Dg..o...#
#.......#.....o......o.....#.......#
#....S..#..................#..S....#
####.############..############.####
#.......#..................#.......#
#..a....#........r.........#....l..#
#.......D...o..........o...D.......#
#...S...#..................#...S...#
#.......#..................#.......#
#%%%.%%%#%%.%%%%%%%%%%%%.%%#%%%.%%%#
#.......#..S............S..#..u....#
#.g.....#.....&&&.&&&&.....#.......#
#.......D.....&..m...&.....D.......#
#...S...#...p.&&&&.&&&.a...#...S...#
#.......#..................#.......#
#%%%.%%%#%%%.%%%%%%%%%%.%%%#%%%.%%%#
#.......#........M.........#.......#
#..l....#....o........o....#....a..#
#.......D..................D.......#
#...S...#..S............S..#...S...#
#.......#......c...........#.......#
#&&&.&&&#&&&&&&&&.&&&&&&&&&#&&&.&&&#
#..m....D.....a...G........D....m..#
#S......#.....S......S.....#......S#
####################################
""",
)

# A goods yard at dusk: offices and a yard to the north, the warehouse in
# the middle with its stacks of crates, a corridor down its west side and
# the loading dock to its east, and the old tunnel under the tracks along
# the south. Three lanes north to south, crossed by the yard and the
# tunnel, doors where they meet the warehouse. Drawn afresh, in the manner
# of the industrial maps players know.
WAREHOUSE = Arena(
    name="warehouse",
    title="THE WAREHOUSE",
    music="level5",
    walls=(CONCRETE, MOSS, BRICK, CONCRETE),
    drawing="""
####################################
#S.......&....&.....S.............S#
#.....m..&..S.&.........u..........#
#........&....D.......o....o..l....#
#&&.&&........&....................#
#.......S..a..&..M...............S.#
#.............&....................#
##..#######D######..#########...####
#....#...................#.........#
#.S..#.S......m..........#.......S.#
#....#...&&........&&...........a..#
#....#...&&........&&..............#
#..a.#........r.S........#.........#
#....#...................#...&&....#
#....D.......&&&&........#...&&....#
#....#.......&&&&........#....p....#
#.l..#..S........a.......#.........#
#..g.#...................D.........#
#....#...&&........&&....#.....&&..#
#........&&........&&....#.....&&..#
#.S..#........m........S.#.......S.#
#....#...................#..m......#
#%..%%%%%%%%%%%D%%%%%%%%%%%%%%..%%%#
#.........%%.......................#
#.........%%..........a............#
#.......o.....o.....o.....o........#
#....G.................l.....M.....#
#S..........c....S................S#
#..................................#
####################################
""",
)

# A walled courtyard of the castle, for two to four and quick fights: an
# open square with a corner of cover in each quarter, short walls to duck
# behind round the middle, and the rocket launcher out in the open at the
# centre, a short dash from every spawn point.
COURTYARD = Arena(
    name="courtyard",
    title="THE COURTYARD",
    music="level2",
    walls=(CONCRETE, BRICK, MOSS, CONCRETE),
    drawing="""
#####################
#S........S........S#
#....m.........a....#
#..%%%.........%%%..#
#..%......G......%..#
#..%...&.....&...%..#
#......&.....&......#
#....&&&.....&&&....#
#...................#
#...................#
#S..g..M..r.....u..S#
#...................#
#...................#
#....&&&.....&&&....#
#......&.....&......#
#..%...&.....&...%..#
#..%......G......%..#
#..%%%.........%%%..#
#....a.........m....#
#S........S........S#
#####################
""",
)

ARENAS = [BAZAAR, WAREHOUSE, COURTYARD]


def cells(arena):
    rows = arena.drawing.strip("\n").split("\n")
    width = len(rows[0])
    for number, row in enumerate(rows):
        if len(row) != width:
            raise SystemExit(f"{arena.name}: row {number} is {len(row)} wide, "
                             f"not {width}")
    return rows


def check(arena, rows):
    height, width = len(rows), len(rows[0])
    # A solid border: nothing walks off the map
    for x in range(height):
        for y in range(width):
            on_edge = x in (0, height - 1) or y in (0, width - 1)
            if on_edge and rows[x][y] in FLOOR:
                raise SystemExit(f"{arena.name}: open border at {x},{y}")
    # Every open cell reachable from every other
    open_cells = [(x, y) for x in range(height) for y in range(width)
                  if rows[x][y] in FLOOR]
    seen = {open_cells[0]}
    queue = deque([open_cells[0]])
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if rows[nx][ny] in FLOOR and (nx, ny) not in seen:
                seen.add((nx, ny))
                queue.append((nx, ny))
    # A door hangs in a wall: wall on two opposite sides, floor on the others
    walls = set("#%&o")
    for x, y in open_cells:
        if rows[x][y] != "D":
            continue
        across_x = rows[x - 1][y] in walls and rows[x + 1][y] in walls and \
            rows[x][y - 1] in FLOOR and rows[x][y + 1] in FLOOR
        across_y = rows[x][y - 1] in walls and rows[x][y + 1] in walls and \
            rows[x - 1][y] in FLOOR and rows[x + 1][y] in FLOOR
        if not across_x and not across_y:
            raise SystemExit(f"{arena.name}: the door at {x},{y} hangs in no wall")
    unreached = [cell for cell in open_cells if cell not in seen]
    if unreached:
        raise SystemExit(f"{arena.name}: cut off: {unreached[:5]}")
    # Spawn points: room round each, and apart
    spawns = [(x, y) for x, y in open_cells if rows[x][y] == "S"]
    if len(spawns) < 8:
        raise SystemExit(f"{arena.name}: {len(spawns)} spawn points, "
                         "8 players need 8")
    # Nothing a body bumps into beside one: no pillar, lamp or door
    for x, y in spawns:
        crowded = [(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                   if rows[x + dx][y + dy] in "oDl"]
        if crowded:
            raise SystemExit(f"{arena.name}: spawn {x},{y} is crowded")
    for i, a in enumerate(spawns):
        for b in spawns[i + 1:]:
            if math.dist(a, b) < SPAWN_SPACING:
                raise SystemExit(f"{arena.name}: spawns {a} and {b} too close")
    return spawns


def campaign_files():
    """Every level file the campaign (or the benchmark) plays: an arena
    must never be written over one"""
    config = json.loads((ASSETS / "levels" / "config.json").read_text())
    found = {config.get("benchmark_level", "")}

    def walk(node):
        if isinstance(node, dict):
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)
        elif isinstance(node, str) and node.endswith(".json"):
            found.add(node)

    walk(config.get("campaign", {}))
    walk(config.get("levels", []))
    return found


def campaign_maps(levels):
    """The maps those levels are drawn on"""
    maps = set()
    for level in levels:
        path = ASSETS / "levels" / level
        if path.exists():
            maps.add(json.loads(path.read_text()).get("map", ""))
    return maps


def build(arena):
    levels = campaign_files()
    if f"{arena.name}.json" in levels or \
            f"{arena.name}.txt" in campaign_maps(levels):
        raise SystemExit(f"{arena.name}: a campaign level has that name")
    rows = cells(arena)
    spawns = check(arena, rows)
    height, width = len(rows), len(rows[0])
    centre = (height / 2, width / 2)
    wall = dict(zip("#%&o", arena.walls))

    map_rows = []
    for row in rows:
        out = []
        for c in row:
            if c in wall:
                out.append(str(wall[c]))
            elif c == "D":
                out.append("D")
            else:
                out.append("0")
        map_rows.append("".join(out))
    (ASSETS / "maps" / f"{arena.name}.txt").write_text(
        f"height {height}\nwidth {width}\n" + "\n".join(map_rows) + "\n")

    def facing(x, y):
        # Towards the arena's middle
        return round(math.atan2(centre[1] - (y + 0.5),
                                centre[0] - (x + 0.5)), 2)

    first = spawns[0]
    level = {
        "name": arena.title,
        "music": arena.music,
        "map": f"{arena.name}.txt",
        "player": {"position": {"x": first[0] + 0.5, "y": first[1] + 0.5,
                                "theta": facing(*first)}},
        "spawns": [{"x": x + 0.5, "y": y + 0.5, "theta": facing(x, y)}
                   for x, y in spawns],
        "enemies": [],
        "dynamicObjects": [
            {"type": "green_light", "position": {"x": x + 0.5, "y": y + 0.5}}
            for x in range(height) for y in range(width) if rows[x][y] == "l"],
        "pickups": [
            {"type": PICKUPS[rows[x][y]],
             "position": {"x": x + 0.5, "y": y + 0.5}}
            for x in range(height) for y in range(width)
            if rows[x][y] in PICKUPS],
    }
    (ASSETS / "levels" / f"{arena.name}.json").write_text(
        json.dumps(level, indent=4) + "\n")
    print(f"{arena.name}: {height} x {width}, {len(spawns)} spawn points, "
          f"{len(level['pickups'])} pickups")


def main():
    for arena in ARENAS:
        build(arena)


if __name__ == "__main__":
    main()
