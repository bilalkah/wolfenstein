# Sprites and animation

| | |
| --- | --- |
| **When** | August 2024, reworked September 2026 |
| **Commits** | `37597fd` Render static object in game, `6a27909` Process object height, `31770ed` Introduce animation, `c15aade` weapon animations, "Animator is removed because state is better approach", `e024275` Store each animation frame once, `9b20b84` enemies seen from all 8 sides |
| **Code today** | [Textures and animation](../engine/assets.md), [Raycasting](../engine/raycasting.md#placing-sprites), [The 3D renderer](../engine/renderer.md) |

## Problem

Put things into the maze that are not walls: lamps, enemies, pickups;
draw them at the right size and place among the wall columns, animate
them, and, for enemies, show the side the player sees.

## Constraints

- No 3D models: flat pictures (billboards) only.
- Correct occlusion against wall columns without a depth buffer.
- Animations tied to game time, not frame count.
- Later: no allocation per frame or per animation change.

## Approach

- **Billboards** (`37597fd`): the camera finds an object's left and right
  edges on the line perpendicular to the view and turns them into screen
  columns; the object is queued with its distance and sorted among the wall
  strips (the painter's algorithm).
- **Height and elevation** (`6a27909`): each object has a height in wall
  units and stands on the floor; puffs and floating enemies are raised.
- **Animation** (`31770ed`): time-based clips (a frame duration, a
  counter), then (in `c15aade`) driven by the owner's state instead of a
  separate animator: a walking enemy plays its walk clip because it is in
  its walk state.
- **Eight views** (`9b20b84`): Freedoom's monsters are drawn from eight
  sides; a clip that turns has `@2` to `@8` variants, and the view is chosen
  by the angle between the enemy's facing and the viewer (`SideSeen`).
- **Frames stored once** (`e024275`): "Each clip had a folder of its own
  frames, so a frame two clips shared ... was stored as many times. Now
  each weapon, enemy and light keeps one folder of its distinct frames,
  and textures.json lists which of them each clip plays."

## C++ techniques used

- [Non-owning views](../techniques/views.md): an animation is a
  `std::span` into the texture manager's frame ids.
- [Heterogeneous lookup](../techniques/heterogeneous-lookup.md): clip
  names built on the stack, looked up as `std::string_view`.
- [State machines with templates](../techniques/state-machines.md): each
  state owns the animation it plays.

## Key code

- [`LoopedAnimation`'s constructor](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Animation/src/looped_animation.cpp#L55-L85):
  finding the seven other sides of a clip without allocating.
- [`SideSeen`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Math/src/vector.cpp#L273-L279):
  which of eight views a viewer sees.
- [`Renderer3D::RenderObjects`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Graphics/src/renderer_3d.cpp#L448-L492):
  sizing and queuing a sprite.

## Pitfalls

- A sprite's picture must be as wide as its widest frame (lying dead,
  aiming to the side), so its **body** radius is separate from its picture
  width (`9b20b84`).
- A dead enemy is one frame drawn from its front; seen from behind it is
  mirrored, so the body does not seem to turn as the player walks round it.
- All eight sides of a clip must exist and be as long as the front; a
  missing side is a startup error, not a missing picture in play.

## What I'd change

- Pack frames into texture atlases so consecutive sprites share a texture.
- Sort sprites against walls per column rather than by one distance per
  sprite, if sprites standing against walls ever show wrong occlusion.
