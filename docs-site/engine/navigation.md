# Pathfinding

## Purpose

Enemies need to get somewhere through the level's rooms and doors: to the
player, to a spot at their fighting range, to cover, to the next point of
their patrol. The navigation module plans those routes on a grid and tells
each enemy where to head next, while keeping groups from piling into the
same corridor and bodies from wedging against lamps.

Code: `src/NavigationManager/` (`grid_path_finder.cpp`,
`navigation_manager.cpp`).

## Concepts

### A* on a grid

A* finds a shortest path in a graph by expanding nodes in order of
\(f(n) = g(n) + h(n)\): \(g\) the cost from the start, \(h\) an estimate of
the cost to the goal. With a heuristic that never overestimates (here the
straight-line distance), the first time the goal is expanded its path is
optimal. Amit Patel's
[Introduction to A*](https://www.redblobgames.com/pathfinding/a-star/introduction.html)
is the standard illustrated walkthrough.

**Weighted A\*** trusts the heuristic more: with
\(f = (1 - w)\,g + w\,h\) and \(w > 0.5\), the search is pulled towards the
goal and expands fewer nodes, at the price of paths that may be longer
than the shortest (by at most a factor \(w / (1 - w)\)). Here \(w = 0.6\),
so \(f \propto g + 1.5\,h\): paths within 1.5 times the shortest,
usually equal to it on these open rooms.

### A finer grid than the map

A map cell is one unit wide; a corridor is one cell. Planning on map cells
would put every enemy on the corridor's centre line, one behind the other.
The pathfinding grid splits each map cell into 2 by 2 cells
(`kCellSize = 0.5`), so two enemies can pass each other in a corridor and
routes can go round one another.

## How it is implemented here

### The grid and its costs

`NavigationManager::Build` fills the path finder's grid from the map once
per level: a cell is blocked if its map cell is a wall or a **locked**
door (enemies open unlocked doors by walking up to them, but not locked
ones). Every step costs 1, plus extras:

| Extra cost | Why |
| --- | --- |
| Other enemies' cells, and the cell each is about to enter | Blocked for this query: route round them (if that finds nothing, a second query ignores them, so a group can still file through a doorway) |
| The first 16 cells of each other enemy's route | +3 (`kCrowdCost`): where there is another way, a group splits up |
| Cells round a lamp, where a body's centre cannot fit | +4 (`kSqueezeCost`): keep clear of lamps where there is room, squeeze past only where there is not |

### A query

```cpp title="src/NavigationManager/src/navigation_manager.cpp"
vector2d NavigationManager::FindPath(Position2D start, Position2D end,
                                     ObjectId id) {
    Route& route = routes_[ToIndex(id)];
    const auto stay = [&] {
        route.cells[0] = ToCell(start.pose);
        route.size = 1;
        return start.pose;
    };
    if (start.pose.Distance(end.pose) < kCellSize * 0.9) {
        return stay();
    }

    // Round the other enemies if it can; if they fill the way (a doorway),
    // through them, as enemies pass each other
    CollectDynamicObstacles(id);
    const GridCell from = ToCell(start.pose);
    const GridCell to = ToCell(end.pose);
    const bool found =
        path_finder_.FindPath(from, to, obstacles_, cells_, crowded_,
                              kCrowdCost) ||
        (!obstacles_.empty() &&
         path_finder_.FindPath(from, to, {}, cells_, crowded_, kCrowdCost));
    if (!found || cells_.size() < 2) {
        return stay();
    }
    // Skip the start cell: the enemy is already there
    const auto path = std::span(cells_).subspan(1);
    route.size =
        static_cast<std::uint32_t>(std::min(path.size(), Route::kCapacity));
    std::ranges::copy(path.first(route.size), route.cells.begin());
    const auto objects = scene_.GetObjects();
    const double radius = ToIndex(id) < objects.size()
                              ? objects[ToIndex(id)]->GetCollisionRadius()
                              : 0.0;
    return SteerRound(
        start.pose, CellCentre(path[std::min<std::size_t>(1, path.size() - 1)]),
        radius);
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/NavigationManager/src/navigation_manager.cpp#L141-L178){ .excerpt-source }

The answer is **where to head next**, not the whole path: the centre of
the path's second cell, steered round any lamp in the way. Enemies replan
every tick they move (the queries are cheap), so a route bends as the
player moves. Only the first 64 cells of a route are kept per enemy, inline
in an array: they are all anyone looks at (the next cell steers, the first
16 crowd the others, the rest is drawn by the 2D view).

### The search

```cpp title="src/NavigationManager/src/grid_path_finder.cpp"
bool GridPathFinder::FindPath(GridCell start, GridCell goal,
                              std::span<const GridCell> extra_blocked,
                              std::pmr::vector<GridCell>& path,
                              std::span<const GridCell> crowded,
                              float crowd_cost) {
    // ...
    const auto weight = static_cast<float>(heuristic_weight_);
    const auto cost = [weight](float g, float h) {
        return (1.0f - weight) * g + weight * h;
    };
    // ...
    bool found = false;
    while (!open_.empty()) {
        std::ranges::pop_heap(open_, kHigherF);
        const std::int32_t current = open_.back().index;
        open_.pop_back();
        const auto current_i = static_cast<std::size_t>(current);
        if (closed_stamp_[current_i] == generation_) {
            continue;  // an outdated entry for an already expanded cell
        }
        closed_stamp_[current_i] = generation_;
        if (current == goal_index) {
            found = true;
            break;
        }

        const GridCell cell = Cell(current);
        for (const GridCell step : kSteps) {
            const GridCell neighbour{cell.x + step.x, cell.y + step.y};
            if (!Contains(neighbour)) {
                continue;
            }
            const std::int32_t index = Index(neighbour);
            const auto i = static_cast<std::size_t>(index);
            if (!passable(index) || closed_stamp_[i] == generation_) {
                continue;
            }
            const float next_g =
                g_[current_i] + 1.0f + static_cast<float>(extra_cost_[i]) +
                (crowded_stamp_[i] == generation_ ? crowd_cost : 0.0f);
            if (seen_stamp_[i] == generation_ && g_[i] <= next_g) {
                continue;  // already reached at least as cheaply
            }
            seen_stamp_[i] = generation_;
            g_[i] = next_g;
            parent_[i] = current;
            open_.push_back({cost(next_g, Heuristic(neighbour, goal)), index});
            std::ranges::push_heap(open_, kHigherF);
        }
    }
    // ...
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/NavigationManager/src/grid_path_finder.cpp#L110-L201){ .excerpt-source }

- The **open set** is a binary heap in a `std::pmr::vector`, driven by
  `std::ranges::push_heap` and `pop_heap`. A cell reached again more
  cheaply is simply pushed again; the stale entry is skipped when it comes
  off the heap ("lazy deletion"), which avoids a decrease-key operation.
- The grid is **4-connected**: steps along the axes only. Paths are
  staircases on the fine grid, which the enemies' steering (heading for
  the *second* cell's centre) smooths.

### Clearing nothing between queries

A search needs per-cell state: best cost so far, parent, closed or not,
blocked for this query, crowded for this query. Clearing arrays the size of
the grid before every query would cost more than many searches. Instead,
every entry carries a **stamp**, and an entry counts only if its stamp
equals the current query's generation:

```cpp title="src/NavigationManager/src/grid_path_finder.cpp"
void GridPathFinder::NextGeneration() {
    if (++generation_ == 0) {
        // Wrapped around after 2^32 queries: stale stamps could now collide
        std::ranges::fill(blocked_stamp_, 0);
        std::ranges::fill(crowded_stamp_, 0);
        std::ranges::fill(seen_stamp_, 0);
        std::ranges::fill(closed_stamp_, 0);
        generation_ = 1;
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/NavigationManager/src/grid_path_finder.cpp#L99-L108){ .excerpt-source }

Starting a query is one increment. The arrays are allocated once per
level from the level arena (`SetGrid`), so a query allocates nothing.

### When the map changes

A secret wall that slides into place, or out of it, changes which cells are
walls. `NavigationManager::RefreshCell` rereads a map cell into the grid,
and the scene calls it for every cell a secret passed through once it
stops.

## Design decisions and trade-offs

- **In-house A\* instead of a library.** The engine started with the
  author's own [path-planning](https://github.com/bilalkah/path-planning)
  project as a git submodule. `f786c53` replaced it with this grid A*:
  by its commit message, pathfinding made 250 of the 299 heap allocations
  per frame (each query copied the grid twice, each pushed neighbour was a
  new `shared_ptr`, and the path was rebuilt in \(O(n^2)\)). The new one
  keeps the same behaviour (unit steps, \(f = 0.4g + 0.6h\)) with flat
  arrays, stamps and a heap reserved to its exact bound.
- **Replanning every tick.** No path caching or invalidation logic: the
  answer always reflects where the player and the other enemies are now.
  The cost is measured by the profiler's `Pathfinding` section.
- **Weighted, not optimal.** Enemies need plausible routes, not provably
  shortest ones; \(w = 0.6\) cuts expansions.
- **Soft crowding instead of reservation.** Enemies do not reserve cells
  over time (as cooperative pathfinding would); they add a cost to each
  other's next stretch. Simple, and enough for groups of a few.

## Pitfalls

- **Start and goal are always passable**, even inside a wall or another
  enemy: a query from a slightly embedded position still finds a way out.
- **Only 64 cells of a route are kept.** A long route's tail is dropped;
  harmless, since the enemy replans before it gets there.
- **4-connectivity** makes diagonal movement cost \(2\) instead of
  \(\sqrt{2}\) in \(g\), so among equal routes the planner has no
  preference for straighter ones. The steering hides this.

## Possible improvements

- Let queries stop early when a route to the player's cell was found last
  tick and nothing on it changed.
- 8-connected steps with \(\sqrt{2}\) diagonal cost (forbidding corner
  cutting past walls) would give straighter paths on open floor.
- A flow field towards the player, computed once per tick for all enemies
  hunting them, would replace many A* queries with one breadth-first pass.
