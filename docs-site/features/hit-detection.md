# Where shots land

| | |
| --- | --- |
| **When** | 27 and 28 September 2026 |
| **Commits** | `20c414c` Show where shots land and rebalance the arsenal, `e024275` Hit only what shows, and harder in the head, `7524277` Soften the blood, and show hits and headshots, `7524277` Draw each bullet mark as one quad |
| **Code today** | [Weapons and combat](../engine/combat.md), [The 3D renderer](../engine/renderer.md#decals-as-quads) |

## Problem

Shots should hit what the player sees, reward aim (the head), and show
where they landed: blood on an enemy, dust and a lasting mark on a wall,
a marker round the crosshair on a hit.

## Constraints

- The decision is the simulation's, from game state, not from the last
  drawn frame.
- No allocation when shooting, even with a wall full of marks.
- Marks must not slow the renderer down.

## Approach

1. **Feedback first** (`20c414c`): "a puff of blood where a shot hits an
   enemy, of dust where it hits a wall, and a bullet mark that stays on the
   wall face (the newest 32; doors and secret walls keep none)."
2. **Pixel-exact hits** (`e024275`): "A shot hit an enemy anywhere inside a
   box its width, so one that passed beside the head or between the legs
   still counted. Now the game keeps a mask of which pixels show for every
   sprite-sized texture, and a shot hits only if it crosses a pixel that
   shows in the frame the enemy is showing." Where it crosses sets the
   damage: head twice, legs 0.6 times.
3. **Readable hits** (`7524277`): four ticks round the crosshair for a
   fifth of a second, red for a headshot; a bigger burst of blood from the
   head. A balance fix hid in it: "A level shot at a soldier ... struck its
   face, so every shot was a headshot. Soldiers are a tenth bigger ... a
   headshot takes aiming up."
4. **Marks as quads** (`7524277`): "In Chrome on a GPU a wall of marks took
   the frame rate from 750 to 530" when each mark was a two-pixel strip per
   wall column; drawing each mark as one quad brought it back to 740.

## C++ techniques used

- `std::optional<Crossing>` for "where the shot crosses the picture, if it
  does".
- Bit masks packed in `std::vector<std::uint64_t>` per texture.
- A ring of 32 `WallMark`s; decals keyed per mark and merged in the
  renderer ([Ranges algorithms](../techniques/ranges.md) for the lookups).

## Key code

- [`Cross`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/ShootingManager/src/shooting_manager.cpp#L119-L145):
  the board test and the mask.
- [`ResolveOneShot`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/ShootingManager/src/shooting_manager.cpp#L75-L115):
  zone, blood and damage.
- [`AddDecalColumn` and `EnqueueDecals`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Graphics/src/renderer_3d.cpp#L367-L435):
  merging a mark's columns into one quad.

## Pitfalls

- **Unmeasured paths stay slow.** "The benchmark and the soak never shot a
  wall, so no mark was ever measured or checked for allocations": both now
  mark walls before they start.
- **Hit zones are relative to the visible figure**, not the padded frame
  (`SolidRows`), or a short enemy's head would be in empty space.

## What I'd change

- Blood decals on the floor under wounded enemies.
- Marks on doors, which currently keep none because the door slides.
