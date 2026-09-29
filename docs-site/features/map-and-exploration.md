# The map of the level

| | |
| --- | --- |
| **When** | 26 September 2026 |
| **Commits** | `c406a58` Add a map of the explored level, toggled large with M |
| **Code today** | `Graphics/minimap.cpp`, `Camera2D::ExploreView`, `Scene::Explore` |

## Problem

Help the player find their way through larger levels without giving the
level away: a map that shows only what they have seen.

## Constraints

- Exploration must be recorded every frame, cheaply.
- Drawing the map must not allocate (SDL's line and rectangle calls do).
- The explored cells go into saved games.

## Approach

From the commit: "Each frame the camera marks the floor its view reaches,
the walls it ends on and the cells around the player as explored; the
level keeps one flag per cell in its arena. A map in the top right corner
shows only those cells, walls tinted by texture, the pickups seen there
and not yet taken, and the player's position and facing. M shows it large
in the middle of the screen. Enemies and lights are left out."

The developer's full 2D view moved behind `--debug` / `?debug`. Both draw
through a `QuadBatch`, one `SDL_RenderGeometry` call for everything.

<figure markdown="span">
  ![The large map](../assets/screenshots/map.png){ width="520" }
  <figcaption>The map, large (M), with every cell of the first level explored.</figcaption>
</figure>

## C++ techniques used

- One `std::uint8_t` flag per cell in a `std::pmr::vector` in the level
  arena.
- A batch of vertices sized once (`QuadBatch`) instead of SDL's
  per-call temporary arrays.

## Key code

- [`Camera2D::ExploreView`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Camera/src/camera.cpp#L52-L77):
  every fourth ray, sampled every 0.3 units.
- [`Minimap`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Graphics/src/minimap.cpp)

## Pitfalls

- Sampling every fourth ray every 0.3 units can miss a sliver of a cell
  seen at a grazing angle; neighbouring rays cross the same cells almost
  always.
- The results screen's "explored" share counts only cells a character can
  stand in; seeing walls adds nothing.

## What I'd change

- Mark the doors' lock colours and the exit on the map once seen.
