# A deterministic simulation

| | |
| --- | --- |
| **When** | 25 to 29 September 2026 |
| **Commits** | `2beddb5` Make the simulation deterministic: player commands and a fixed timestep, `7524277` Keep the mouse's motion between ticks, so it turns as far at any frame rate, `d41e706` Aim where you look: the view turns with the mouse at once |
| **Code today** | [Main loop and timing](../engine/main-loop.md), [Input](../engine/input.md) |

## Problem

"The simulation read the keyboard and mouse itself, resolved shots from
the ray the camera cast while drawing, and stepped by the variable frame
time, so the same play gave different results at different frame rates and
could not be scripted, replayed or run without a view" (`2beddb5`).

## Constraints

- Identical results for identical input, at any frame rate.
- Smooth motion on screens faster than the simulation.
- Mouse look that feels immediate.

## Approach

1. **Commands** (`2beddb5`): "Input is sampled once per frame into a
   PlayerCommand ...; the player only executes commands and no longer
   touches SDL input or the settings."
2. **Shots from game state**: "`Aim(scene, eye)` resolves a shot from the
   game state when it is fired ...; the camera only draws, and the player
   and weapon hold no reference to it."
3. **A fixed 60 Hz tick** with interpolated drawing.
4. **Gathering input between ticks** (`7524277`): a tick used to take only
   the last frame's command, so fast screens lost mouse motion.
5. **View angles on the presentation side** (`d41e706`): the view turns
   with the mouse every frame and the tick takes the angles, instead of the
   view lagging a tick or two behind the hand.

## C++ techniques used

- A plain aggregate `PlayerCommand` with a defaulted `operator==`, so
  tests compare commands directly.
- Seeded, per-object random generators (enemies' xorshift, drops'
  SplitMix64) instead of a global one.

## Key code

- [`PlayerCommand` and `Gather`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/include/Characters/player_command.h)
- [`FixedStep::Advance`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/TimeManager/src/time_manager.cpp#L39-L47)
- [`ViewAngles::Apply`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/include/Characters/view_angles.h#L33-L46)

## Pitfalls

- **The order inside the player's tick**: shooting and moving before
  `Rotate()` uses the previous tick's view; see
  [One frame](../architecture/frame-lifecycle.md#pitfalls-in-this-order).
- **Floating point is deterministic only on the same build**: a replay
  recorded natively may not reproduce exactly in the browser (different
  optimisation, `-ffast-math` or not, library functions like `sin`).

## What I'd change

- Record sessions as command streams and replay them in CI.
- Move `Rotate()` first in `Player::Update`.
