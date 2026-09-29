# Enemy pathfinding

| | |
| --- | --- |
| **When** | August 2024, rewritten September 2026 |
| **Commits** | `cfeddd5` Ft add path planning (the `path-planning` submodule), `28e468b` Update path-planning, `f786c53` Replace the path-planning submodule with an allocation-free grid A*, `7524277` Route enemies clear of lamps and each other, and never wedge them, `2a2590c` (crowd costs) |
| **Code today** | [Pathfinding](../engine/navigation.md) |

## Problem

Enemies must reach the player (and later, spots round the player, cover,
patrol points) through rooms, corridors and doors, without walking into
walls, lamps or each other.

## Constraints

- Replanning every tick for every hunting enemy, within a frame budget.
- After September 2026: **no allocation per query**.
- Groups should not file down one corridor or wedge against a lamp.

## Approach

- **2024**: A* from the author's own
  [path-planning](https://github.com/bilalkah/path-planning) library, as a
  git submodule, on the map's grid.
- **The rewrite** (`f786c53`). Measurements came first: "Pathfinding made
  250 of the 299 heap allocations per frame: every query copied the
  pathfinding grid, A* copied it again, and every neighbour it pushed was a
  new `shared_ptr<NodeParent>`, with the path rebuilt in O(n²) and each
  expansion logged under a mutex." The replacement keeps the behaviour
  ("unit steps, f = 0.4 g + 0.6 h, start and goal always passable") with
  flat per-cell arrays sized once per level, a generation counter instead of
  clearing, a binary heap reserved to its exact bound (four pushes per cell
  plus one), and an O(n) path rebuild. It also fixed undefined behaviour:
  "FindPath no longer calls `front()` on an empty vector when a path has
  exactly two cells".
- **A finer grid** of half-cells (`kCellSize = 0.5`), so two enemies fit
  side by side in a corridor; "Map no longer knows about pathfinding".
- **Lamps and crowds** (`7524277`, `2a2590c`): cells a lamp crowds cost
  more; other enemies' next cells are blocked for a query (with a second
  query ignoring them if that finds nothing); the first stretch of other
  enemies' routes costs more, so groups split up.

## C++ techniques used

- [Polymorphic memory resources](../techniques/pmr-arenas.md): every array
  from the level arena.
- [Ranges algorithms](../techniques/ranges.md): `push_heap`, `pop_heap`,
  `fill`, `reverse`, `copy`.
- [Templates and concepts](../techniques/templates.md): `SetGrid` takes any
  `std::predicate<int, int>`.
- [`std::span`](../techniques/views.md) for obstacles and crowded cells.

## Key code

- [The A* search](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/NavigationManager/src/grid_path_finder.cpp#L110-L201)
- [Generations instead of clearing](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/NavigationManager/src/grid_path_finder.cpp#L99-L108)
- [A query for one enemy](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/NavigationManager/src/navigation_manager.cpp#L141-L178)

## Pitfalls

- "An enemy's own next cell was also among the obstacles it planned round,
  turning it off its route every other query" (`7524277`): obstacles now
  skip the querying enemy.
- "Enemies filling a doorway left those behind with no route at all": the
  second query ignoring other enemies fixed it.
- Routes kept per enemy were first reserved for a path through every free
  cell, "17 KB per enemy"; they are now 64 cells inline.

## What I'd change

- A flow field towards the player, shared by every enemy hunting them.
- 8-connected steps for straighter routes in open rooms.
