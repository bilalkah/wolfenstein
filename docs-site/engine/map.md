# The map

## Purpose

`Map` is the level's geometry: a grid of cells, each free floor, a wall
with a texture, or a door; plus the sliding doors' state, the secret walls
that can be pushed, and the exit switch. Everything spatial asks it:
raycasting, collision, line of sight, pathfinding, noise, the minimap.

Code: `src/GameMap/include/GameMap/map.h`, `src/GameMap/src/map.cpp`;
map files in `assets/maps/*.txt`.

## Concepts

### Tile maps

A tile map stores a world as a grid of small integers. It is compact
(a 30 by 40 level is 1200 cells) and it turns spatial questions into index
arithmetic: the cell under a point \((x, y)\) is
\((\lfloor x \rfloor, \lfloor y \rfloor)\), its neighbours are one index
away. *Wolfenstein 3D* used a 64 by 64 grid of wall and door codes.

### Coordinates

In this engine, **x is the row and y the column**: cell `(x, y)` is row
`x` of the map file, character `y`. A cell's centre is \((x + 0.5,
y + 0.5)\). Angles follow from `atan2(dy, dx)`, so angle 0 points down the
rows of the file (+x) and \(\pi/2\) along them (+y).

## How it is implemented here

### The file format

```text title="assets/maps/level1.txt (first rows)"
height 22
width 30
111111111111111111222522222211
100000011000000001200000022211
100000011000000001200000022211
10000000D000000001200000022211
100000011000000000D00000022211
111011111000000001200000022211
```

| Character | Cell |
| --- | --- |
| `0` | Free floor |
| `1` to `5` | A wall, with wall texture 1 to 5 (`walls` in `textures.json`) |
| `D` | A door |
| `G`, `S` | A door locked with the gold or the silver key |
| `X` | The exit switch: a wall drawn with the exit texture; there is at most one |

The files are generated from room layouts by `scripts/make_levels.py`
(see [Levels and content data](levels.md)).

### Cells in memory

```cpp title="src/GameMap/include/GameMap/map.h"
// The level's grid of cells: 0 is free, a door cell is kDoorCell plus the
// door's index, anything else a wall whose value is its texture id. The
// exit switch (X in a map file) is a wall drawn with the kExitWall texture. Cells
// are stored in one row-major block (a vector of rows would allocate once
// per row and scatter them across the heap) and read through std::mdspan,
// so cell (x, y) is GetCells()[x, y].
class Map
{
  public:
    using CellView =
        std::mdspan<const std::uint16_t, std::dextents<std::size_t, 2>>;

    // Door cells are numbered from here; wall textures stay far below
    static constexpr std::uint16_t kDoorCell = 0x8000;
    // The wall texture of the exit switch
    static constexpr std::uint16_t kExitWall = 6;
    // A door this far open lets characters and sight through
    static constexpr double kPassableOpenness = 0.8;
    static constexpr bool IsDoorCell(std::uint16_t cell) {
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/GameMap/include/GameMap/map.h#L69-L87){ .excerpt-source }

One `uint16_t` per cell encodes all three kinds: 0 is floor, a small
number is a wall's texture id, and `0x8000 + i` is door number `i`, whose
state lives in a separate `Door` array. The cells are one contiguous
`std::pmr::vector`, so a level's copy of the map lives in the level arena.

### Parsing

`Map::FromFile` reads the header and the rows and checks everything a
later system would trip over: rows of the right length, known characters,
at most one exit, and every door standing between two walls facing each
other with the way through open on its other two sides. That last check
also decides which way the door faces:

```cpp title="src/GameMap/src/map.cpp"
        for (const char c : line) {
            // D a door; G and S doors locked with the gold and silver keys
            if (c == 'D' || c == 'G' || c == 'S') {
                const auto column = map.cells_.size() % map.size_y_;
                map.cells_.push_back(
                    static_cast<std::uint16_t>(kDoorCell + map.doors_.size()));
                map.doors_.push_back({.x = static_cast<std::uint16_t>(rows),
                                      .y = static_cast<std::uint16_t>(column),
                                      .lock = c == 'G'   ? KeyColour::Gold
                                              : c == 'S' ? KeyColour::Silver
                                                         : KeyColour::None});
                continue;
            }
            if (c == 'X') {
                if (map.has_exit_) {
                    return std::unexpected(path + ": more than one exit");
                }
                map.has_exit_ = true;
                map.exit_x_ = static_cast<int>(rows);
                map.exit_y_ = static_cast<int>(map.cells_.size() % map.size_y_);
                map.cells_.push_back(kExitWall);
                continue;
            }
            if (c < '0' || c > '5') {
                return std::unexpected(path + ": unknown cell '" +
                                       std::string(1, c) + "'");
            }
            map.cells_.push_back(static_cast<std::uint16_t>(c - '0'));
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/GameMap/src/map.cpp#L69-L96){ .excerpt-source }

Errors come back as `std::expected<Map, std::string>`, naming the file and
the row; see [Errors as values](../techniques/expected.md).

### Blocked, wall, door

Three questions sound alike and differ:

- `IsWall(x, y)`: blocked for good (a wall, or outside the map).
- `IsBlocked(x, y)`: blocked *now*: a wall, a door less than 80% open, or a
  cell a sliding secret is passing through. Movement, line of sight and
  noise use this.
- `FindDoor(x, y)`: the door in that cell, if any.

```cpp title="src/GameMap/src/map.cpp"
bool Map::IsBlocked(int x, int y) const {
    if (!Contains(x, y)) {
        return true;
    }
    // A sliding secret fills every cell of its way until it stops
    for (const PushWall& wall : push_walls_) {
        for (int step = 0; wall.moving && step <= PushWall::kDistance; ++step) {
            if (x == wall.x + step * wall.dx && y == wall.y + step * wall.dy) {
                return true;
            }
        }
    }
    const std::uint16_t cell =
        GetCells()[static_cast<std::size_t>(x), static_cast<std::size_t>(y)];
    if (IsDoorCell(cell)) {
        return doors_[cell - kDoorCell].openness < kPassableOpenness;
    }
    return cell != 0;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/GameMap/src/map.cpp#L160-L178){ .excerpt-source }

Cells outside the map count as blocked, so nothing walks, sees or hears
past its edge; that fixed a line of sight that could loop forever
(`9a55a11`, "Bounds-check map access and stop line of sight from looping
forever").

### Doors

A door's `openness` goes from 0 (closed) to 1 (open). The `Scene` moves it
(open over half a second, stay open four, close again unless someone
stands in the doorway, reopen if someone steps in while it closes). The
map only stores it; the raycaster draws the door as a plane across the
middle of its cell, slid aside by `openness` (see
[Raycasting](raycasting.md)).

### Secret walls

A `PushWall` is a wall that, when used, slides two cells back along
\((dx, dy)\) and stays there, opening a hidden room. Pushing takes it out
of the grid (its cell becomes 0); while it moves it blocks every cell of
its way and the raycaster draws it as a moving box; when it arrives, its
texture is written into the cell where it stops:

```cpp title="src/GameMap/src/map.cpp"
void Map::AdvancePushWalls(double cells) {
    constexpr double kEnd = PushWall::kDistance;
    for (PushWall& wall : push_walls_) {
        if (!wall.moving) {
            continue;
        }
        wall.offset = std::min(wall.offset + cells, kEnd);
        if (wall.offset >= kEnd) {
            wall.moving = false;
            const int x = wall.x + PushWall::kDistance * wall.dx;
            const int y = wall.y + PushWall::kDistance * wall.dy;
            cells_[(static_cast<std::size_t>(x) * size_y_) +
                   static_cast<std::size_t>(y)] = wall.texture;
        }
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/GameMap/src/map.cpp#L231-L246){ .excerpt-source }

The comment on `PushWall` explains why two cells: "so the block does not
end in front of the gap it leaves". When a secret stops, the `Scene`
refreshes the pathfinding grid for the cells it passed through
(`NavigationManager::RefreshCell`), so enemies route through the opened
room.

## Design decisions and trade-offs

- **One integer per cell, doors in a side table.** Walls cost nothing
  beyond the grid; doors carry state (openness, lock, orientation) only
  where there are doors.
- **Map files as text.** Readable and diffable in git; they are generated,
  so their terseness costs no one typing.
- **Validated once, at load.** A map that loads is one every system can
  trust; nothing checks for impossible doors later.
- **A copy per level, in the arena.** `Map(const Map&, memory_resource*)`
  copies the prepared map into the level's arena, so a level can change its
  map (doors, pushed secrets) while the prepared original stays pristine
  for the next time the level is played.

## Pitfalls

- **x is the row.** Every `(x, y)` in the code and the level files is
  (row, column), not (horizontal, vertical); `make_levels.py` notes the
  same convention. Mixing them up transposes a level.
- `IsBlocked` walks the list of push walls on every call. Levels have one
  or two, so this is cheap, but it runs for every DDA step of line of
  sight and every collision check.
- Map files allow wall textures 1 to 5 only (the parser rejects other
  digits), matching the five `walls` entries before the exit texture in
  `textures.json`.

## Possible improvements

- Keep the moving push walls' cells in a small bitmask instead of scanning
  the push wall list in `IsBlocked`.
- Allow more wall textures (two characters per cell, or letters), which
  the `uint16_t` cells already have room for.
