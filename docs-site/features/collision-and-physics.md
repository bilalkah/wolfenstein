# Bodies that collide

| | |
| --- | --- |
| **When** | 2024, then 26 and 29 September 2026 |
| **Commits** | `5240c81` Add collision distance, `ea93fbe` Stop bodies passing through living enemies and lamps, `6f2e0da` Slide round enemies and lamps instead of stopping short, `e8e9914` Keep bodies out of wall corners, `d41e706` Tighten the physics |
| **Code today** | [Collision and movement](../engine/collision.md) |

## Problem

The player and enemies walked through each other and through lamps,
stopped short of what they touched, cut into wall corners at an angle,
and big enemies stood half in walls.

## Constraints

- No physics engine and no velocities: a wished step per tick, trimmed by
  collision.
- Nothing may stick: a body must always be able to move away.
- Groups must still get through doorways.

## Approach

Each commit fixed one thing, and its message says which:

1. **Solid bodies** (`ea93fbe`): "Every object has a collision radius, 0
   for what can be walked through ... The player is stopped by living
   enemies and lamps; enemies by the player and lamps, not by each other, or
   they would jam in doorways."
2. **Sliding round** (`6f2e0da`): "walking at an enemy or a lamp stopped the
   player short of it ... Now ... its end is pushed out of whatever it would
   enter, to that thing's edge and straight away from its centre, so the
   body ends touching it and keeps the part of its move along it."
3. **Corners** (`e8e9914`): "The wall test checked one point ahead of a
   body along the axis it moved on, so a wall corner off that line went
   unseen ... The body is now a square the collision distance each way, and
   the edge it moves towards must be clear at both its corners."
4. **Tightening** (`d41e706`): every body meets walls at its own size ("a
   demon or the cyber demon stood in it"); enemies standing in one another
   ease apart a little each tick; diagonal movement is no faster than
   straight; rockets burst on lamps; a sliding secret changes the enemies'
   routes as it goes.

## C++ techniques used

- Free functions of what they read (`CheckWallCollision(map, pose, delta,
  radius)`, `PushOutOf`), testable without a scene (`c9fb49e` turned the
  collision "manager" into functions).
- `std::span<IGameObject* const>` for the objects to resolve against.

## Key code

- [`CheckWallCollision`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/CollisionManager/src/collision_manager.cpp#L6-L32)
- [`PushOutOf`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/CollisionManager/src/collision_manager.cpp#L34-L49)
- [`Enemy::KeepApart`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/enemy.cpp#L203-L233)

## Pitfalls

- A standing enemy eased apart must keep its new place: "Standing, it
  stands where it was eased to; walking, it goes on" (`KeepApart`),
  otherwise its target pose pulls it back.
- The order is bodies first, then walls one axis at a time; a push out of
  an enemy that points into a wall is refused on that axis.

## What I'd change

- A spatial grid of bodies if levels grow to many more enemies.
