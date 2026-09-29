# Rockets and plasma

| | |
| --- | --- |
| **When** | 28 September 2026 |
| **Commits** | `4f55a94` Fire rockets and plasma: a rocket launcher and a plasma rifle |
| **Code today** | [Weapons and combat](../engine/combat.md#projectiles), `GameObjects/projectile.h`, `Scene::Launch`, `Fly`, `Burst` |

## Problem

Add weapons whose shots **fly**: a rocket that can be seen, dodged, and
bursts with a blast that hurts everyone near it, the shooter included; and
a plasma bolt that travels fast and hurts only what it hits.

## Constraints

- Firing, flying and bursting allocate nothing.
- A fast projectile must not pass through a thin obstacle between two ticks.
- The blast must respect walls: nobody behind a wall is hurt.
- Wounding (pain, cries, counting kills once) must behave the same as for
  hitscan shots.

## Approach

From the commit message:

- "A rocket or a bolt goes straight on at its speed, seen from the side it
  is seen from, and bursts on the first wall, closed door or living enemy
  in its way."
- "A rocket's blast hurts whoever is in reach with nothing in between,
  weaker further off, and the player too if they fire too close (half the
  damage)."
- "The scene keeps 16 projectiles for a level's life and reuses them, as
  it does the puffs where shots land, so firing, flying and bursting
  allocate nothing."
- "Hurting an enemy (its pain, its cry, a kill counted once) is the scene's
  now, shared by shots and blasts" (`Scene::Wound`).

Flight moves each projectile in steps of 0.05 units inside a tick (shorter
than any body is wide), testing walls, enemies and lamps at each step.

## C++ techniques used

- A fixed `std::array<Projectile, 16>` ring, objects joining the scene's
  object list once, at load ([Game objects and the scene](../engine/entities.md)).
- `std::optional<ProjectileConfig>` in `WeaponConfig`: a weapon fires a
  projectile if it has one.
- A local lambda returning `std::optional<double>` for "the blast's damage
  at this body, if it reaches it" in `Scene::Burst`.

## Key code

- [`Projectile`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/GameObjects/include/GameObjects/projectile.h):
  launched, moved and stopped by the scene; interpolated when drawn.
- `Scene::Fly` and `Scene::Burst` in
  [`scene.cpp`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene.cpp).

## Pitfalls

- **Bursting short.** A rocket meeting a wall bursts where it was, a step
  before the wall, so its blast is not inside the wall (where line of sight
  from the burst would fail).
- **Recycling.** With 16 in flight, the 17th reuses the oldest, which
  vanishes mid-flight; the fire rates keep far fewer in the air.
- **Self-damage.** A rocket fired at a wall at point blank hurts the
  player (half); intended, "enough to teach care, not to end a game at a
  wall".

## What I'd change

- Swept collision (a segment against circles and the grid) instead of
  fixed steps, if projectiles get much faster.
- Knock-back from the blast.
