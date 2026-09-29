# Enemy senses

| | |
| --- | --- |
| **When** | 28 September 2026 |
| **Commits** | `28b59ee` Let enemies hear gunfire, `7524277` Let an enemy's gunfire bring the others within earshot, `9b20b84` Let an enemy that is shot cry out to those near it, `9b20b84` Let enemies walk about their rooms, and guards stand and look around, `9b20b84` Keep enemies moving between states, `e024275` Let enemies shoot back under steady fire |
| **Code today** | [Enemy AI](../engine/ai.md) |

## Problem

Enemies that only react to a player in plain sight are easy to pick off
one by one from a distance, and stand frozen until found. A level feels
alive when enemies move about, hear a fight, and come to it.

## Constraints

- Sound must travel as it would in the level: round corners, not through
  walls or closed doors.
- No allocation per noise.
- The same run every time (patrol spots chosen deterministically).

## Approach

- **Hearing** (`28b59ee`): "A shot is a noise: it spreads through open
  floor a cell a step, round corners but not through walls or closed
  doors, as far as the weapon's noise_range ..., and alerts every living
  enemy it reaches. An alerted enemy comes hunting the player, seen or not,
  for alert_seconds (10 by default)." The flood fill uses buffers the level
  sets aside.
- **Enemies' own shots** carry too (`7524277`): "A soldier's rifle carries
  10 cells and the cyber demon's laser 12; the caco demon's bite makes no
  sound."
- **Cries** (`9b20b84`): "from far enough off the player could pick enemies
  off one by one, their neighbours none the wiser ... An enemy hit now
  cries out, killed or not: every living enemy within its cry (6 cells
  round corners ...) comes hunting."
- **Patrols and guards** (`9b20b84`): patrollers walk to spots in sight of
  their post, never through a wall; guards stand and look one way, then
  another.
- **No stalls between states** (`9b20b84`): patrollers never pass through
  `Idle`; a hunter with no route and no sight "has lost the trail, and goes
  back to walking about ... at once".
- **Shooting back** (`e024275`): "hitting it faster than once a second kept
  it from ever firing"; now a flinch at most every 1.2 s, and it fires back
  at once out of its pain.

## C++ techniques used

- A breadth-first flood fill over fixed `std::pmr::vector` buffers (a
  queue as an array with head and tail indices), sized once per level.
- Per-enemy xorshift generators for patrol choices (deterministic).

## Key code

- [`Scene::MakeNoise`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/scene.cpp#L138-L188):
  the flood fill.
- [`Enemy::NoticesPlayer`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Characters/src/enemy.cpp#L261-L268):
  heard, or seen near, in any direction.
- [`Enemy::TakeHit`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Characters/src/enemy.cpp#L243-L255):
  the pain cooldown.

## Pitfalls

- Freedoom's monsters have no standing frame of their own: "Freedoom's
  standing frames are two steps of its walk" (`9b20b84`), so a guard
  stands on one frame rather than cycling.
- A noise that reaches an enemy behind a locked door alerts it, but it has
  no way through; the lost-trail rule sends it back to its round.

## What I'd change

- Remember where a noise came from and investigate that spot, rather than
  hunting the player's actual position.
