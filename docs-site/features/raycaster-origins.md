# The first raycaster

| | |
| --- | --- |
| **When** | February to August 2024 |
| **Commits** | `b1e85fa` Initial commit, `2952f86` "One big commit :)", `ef50b73` Fix fishbowl effect, `dd264df` Render sky, `a303f28` "adjust the codebase - 1d works", `4d25943` Add simple 3d renderer, `c083d1a` Add color, `1f6a29f` Introduce texture renderer, `5240c81` Add collision distance, `d72c3a6` texture struct, a shorter view distance and black beyond it |
| **Code today** | [Raycasting and the camera](../engine/raycasting.md), [The 3D renderer](../engine/renderer.md) |

## Problem

Draw a first-person 3D view of a maze on a machine that has only a 2D
drawing API (SDL's renderer), the way *Wolfenstein 3D* did: walls of equal
height on a grid, seen from a player who can turn and walk.

## Constraints

- SDL2 as the only graphics layer: rectangles, lines and texture copies,
  no 3D pipeline.
- A grid world: every wall is a whole cell.
- Real time.

## Approach

The project grew in the classic order of raycaster tutorials:

1. **A 2D top-down view** first, with rays drawn from the player into the
   grid ("1d works", `a303f28`).
2. **A 3D view from the same rays** (`4d25943`): each ray's distance
   becomes a vertical line, taller the nearer the wall.
3. **The fishbowl fix** (`ef50b73`): walls bulged towards the middle of
   the screen until the height came from the perpendicular distance,
   \(t \cos(\phi - \theta)\), instead of the distance along the ray.
4. **Colour per wall** (`c083d1a`): the early renderer set a draw colour by
   wall type (red, green, blue, yellow) and drew filled lines.
5. **Textures** (`1f6a29f`): each column became a one-texel-wide strip of a
   wall texture, picked by where the ray hit the wall, and drawn scaled
   with `SDL_RenderCopy`. `d72c3a6` stored each texture's size with it and
   drew solid black beyond a shorter view distance.
6. **A sky** (`dd264df`) behind everything.
7. **Collision** (`5240c81`): the player keeps a distance from walls.

Everything since has built on those choices: the DDA, one ray per two
columns, the column strips drawn by the GPU through SDL, and distance-based
sorting.

## C++ techniques used

The 2024 code predates most of the techniques in this site; it was brought
to C++20 (`cca8f89`) and later C++23 (`f84975d`), and reshaped in
September 2026 (see [A World that owns the game](world-and-ownership.md)
and [The toolchain and CI](toolchain-and-ci.md)).

## Key code

The DDA and the fishbowl correction as they are today:
[the DDA loop](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Camera/src/raycaster.cpp#L99-L144)
and [the per-column correction and texture strip](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp#L228-L254),
both explained in [Raycasting and the camera](../engine/raycasting.md).

## Pitfalls

- The fishbowl effect is the first bug every raycaster meets; the fix is
  one cosine, but the variable names kept a trace of the confusion: a ray's
  `perpendicular_distance` is still the distance along the ray until the
  renderer multiplies it by the cosine.
- This first version spread its rays at equal angles rather than through
  even steps on a camera plane. That stayed until `dafd4e8` (September
  2026), and bent long walls at wide fields of view (see
  [Spreading the rays](../engine/raycasting.md#spreading-the-rays)).

## What I'd change

- The projection is done: the rays go through a camera plane since
  `dafd4e8`. One ray per two columns remains, which halves the work;
  [Raycasting](../engine/raycasting.md#possible-improvements) lists what
  could follow (a ray per column at high resolutions, textured floors).
