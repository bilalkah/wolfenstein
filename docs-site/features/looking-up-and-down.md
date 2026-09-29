# Looking up and down

| | |
| --- | --- |
| **When** | 28 and 29 September 2026 |
| **Commits** | `de89d1c` Look up and down, `de89d1c` Look up and down at the speed of turning, under an unstretched sky, `9b20b84` show the whole sky looking up, `d41e706` Join the sky up all the way round; widen the view to 80 degrees at most, `7524277` (field of view setting) |
| **Code today** | [The 3D renderer](../engine/renderer.md), [Input](../engine/input.md#mouse-sensitivity-in-pixels-on-screen) |

## Problem

A raycaster has no real vertical camera, yet players expect the mouse to
look up and down, and shots to follow the aim.

## Constraints

- The renderer draws vertical wall strips around a horizon; it cannot tilt
  the camera.
- Shots must go where the crosshair is.
- The sky must look right wherever the player looks.

## Approach

- **Shearing, not tilting** (`de89d1c`): "The view slides with the look, as
  in the raycasters of old (the crosshair stays mid-screen), up to 0.4 of
  the screen either way." Walls, sprites and the sky move down the screen
  by `pitch * pixels_per_unit`.
- **Shots follow the aim**: "a shot climbs or falls as it flies, from the
  eye half a wall up, so one aimed over an enemy's head or into the floor
  before it misses."
- **The same speed as turning** (`de89d1c`): "A mouse pixel now slides the
  view as far up or down as it turns it ... where before it moved six times
  slower."
- **The sky** (`de89d1c`, `9b20b84`, `d41e706`): kept at its natural size
  instead of stretching; drawn tall enough to reach the top of the view at
  full pitch; a whole number of repeats per turn so it "joins up wherever
  the player looks"; above it, the colour of its top edge, into which the
  edge fades.
- **The field of view** (`7524277`, `d41e706`): a setting up to 100
  degrees, then "80 degrees rather than 100: wider, the view distorted too
  much".

## C++ techniques used

- `std::clamp` for the pitch, `std::fmod` and `std::round` for the sky's
  layout; a small value type (`SkyLayout`) with designated initialisers.

## Key code

- [`LaySky`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Graphics/include/Graphics/renderer_interface.h#L70-L80)
- [`ToMouseLook`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/game.cpp#L675-L688)
- [`CalculateVerticalSlice`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Graphics/src/renderer_3d.cpp#L504-L521)

## Pitfalls

- Shearing is not perspective: looking far up, walls stay vertical instead
  of converging, which is why the look is limited to 0.4 of the screen.
- A sky that does not repeat a whole number of times a turn jumps where the
  view's angle wraps round (`d41e706`).

## What I'd change

- Nothing structural; the shear is the classic, cheap answer for this kind
  of renderer.
