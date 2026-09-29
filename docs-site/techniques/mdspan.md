# `std::mdspan`

## The technique

`std::mdspan` (C++23) is a non-owning **multidimensional view** over a
contiguous block: a pointer plus extents (and a layout mapping, row-major
by default). It lets a flat array be indexed as a grid without writing
`data[x * width + y]` by hand, and C++23 lets `operator[]` take several
arguments, so the access reads `cells[x, y]`.

```cpp
using CellView = std::mdspan<const std::uint16_t,
                             std::dextents<std::size_t, 2>>;
CellView cells(data, rows, columns);
std::uint16_t c = cells[x, y];          // row x, column y
std::size_t rows = cells.extent(0);
```

`std::dextents<std::size_t, 2>` means two dynamic extents of type
`std::size_t`; static extents (`std::extents<std::size_t, 64, 64>`) would
fix the size at compile time.

## Where it appears here

The map stores its cells in **one row-major block**, not a vector of rows,
and exposes them through an mdspan:

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

The DDA loop, the hottest code in the engine, reads cells as
`cells[map_check.x, map_check.y]` and the extents as `cells.extent(0)`
and `cells.extent(1)` (see [Raycasting](../engine/raycasting.md)).

The change came with `8ffbef1` ("Map is one row-major block read through
`std::mdspan`"); the old map was a vector of rows, and a raycaster that
took it by value copied every row, every frame.

## Why libc++

`<mdspan>` is one reason the whole project builds with Clang and libc++:
"libstdc++ lacks `<mdspan>` even in GCC 15" (`scripts/install_deps.sh`).
Emscripten and macOS use libc++ already, so native Linux builds use it too.

## Pitfalls

- **No bounds checking.** `cells[x, y]` with an out-of-range index is
  undefined behaviour, as with raw arrays. The map's own accessors check
  `Contains(x, y)` first; the DDA loop checks the indices before reading.
- **Row-major means x is the slow index.** Iterating `for x { for y }`
  walks memory in order; the other way round jumps a row each step.
