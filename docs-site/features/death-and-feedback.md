# Dying and hurting

| | |
| --- | --- |
| **When** | 28 September 2026 |
| **Commits** | `64ee770` Fall to the floor when killed, `28b59ee` Fall onto the left side, not straight down, `9b20b84` Keep the dead lying where they fell, `7524277` hit markers, `59288f3` the pickup flash |
| **Code today** | [The player](../engine/player.md#being-hurt-and-dying), [The 3D renderer](../engine/renderer.md) |

## Problem

Death cut straight to a game-over screen; hits and pickups gave little
feedback; enemies' bodies turned to face the viewer as the player walked
round them.

## Constraints

- Everything drawn with the same renderer, no allocation.
- The HUD and the damage overlay stay readable while the view falls.

## Approach

- **Falling** (`64ee770`): "over 0.9 s, slowly at first as things fall, the
  eye drops from half a wall up to near the floor ... the gun slides out of
  sight, and the body lands with a thud ... Game over comes once it has
  landed." The renderer takes the eye's height as a parameter.
- **Rolling** (`28b59ee`): "the world is drawn into a texture made with the
  renderer and drawn back rolled clockwise, up to 80 degrees as the body
  lands, scaled to leave no corner of the screen bare. The damage overlay
  and the HUD stay upright over it."
- **Bodies that stay put** (`9b20b84`): "Freedoom draws a death from its
  front only, so a body lying down turned with the viewer. It now lies
  across the way the enemy faced as it died (it turns to its killer): seen
  from behind it is mirrored."
- **Feedback**: the red damage overlay fading after a hit, a gold flash on
  a pickup, four ticks round the crosshair on a hit (red for a headshot).

## C++ techniques used

- [RAII and custom deleters](../techniques/raii.md): the off-screen
  texture the dying view is drawn into is a `std::unique_ptr` with an SDL
  deleter, created at startup.

## Key code

- [`Player::GetDeathFall` and `GetEyeHeight`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Characters/src/player.cpp#L257-L270)
- [`Renderer3D::RenderFallen`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp#L578-L599)

## Pitfalls

- Render targets may be unavailable on some renderers; then "they fall
  without rolling" (the comment on `fallen_view_`).
- A dead player must not be hurt further or seen by enemies:
  `Player::DecreaseHealth` returns at once for a dead player ("fallen:
  nothing more hurts it"), and enemies stop seeing, hunting and shooting
  once the player has fallen (`Enemy::Update`).

## What I'd change

- A short slow-motion or fade before the game-over screen, if the design
  wants one.
