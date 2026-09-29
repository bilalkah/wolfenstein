# Combat tactics

| | |
| --- | --- |
| **When** | 28 September 2026 |
| **Commits** | `2a2590c` Let each kind of enemy fight from its own range, `2a2590c` Step aside between shots, `2a2590c` Break off and run for cover when badly hurt, `2a2590c` Spread out round the player instead of filing up to them, `2a2590c` Take turns to shoot |
| **Code today** | [Enemy AI](../engine/ai.md#hunting-and-tactics), `tests/tactics_test.cpp` |

## Problem

"Every hunting enemy used to walk up to a player it saw and stop an arm's
length away, rifle or teeth" (`2a2590c`). A room of enemies all fired at
once, stood still between shots, and piled up in the same doorway.

## Constraints

- Behaviour configurable per enemy type in `config.json`.
- Built from rules each enemy applies on its own (no squad coordinator).
- Testable in small maps.

## Approach

Five rules, each a commit:

1. **A range per type** (`2a2590c`): `"range": [near, far]`. Further than
   far: close in. Nearer than near: back away, gun on the player. In
   between: stand and turn. "Soldiers fight across a room, the minigun
   zombie and the cyber demon from further back, the shotgun zombie up
   close, and the demons come right up to bite."
2. **Sidesteps** (`2a2590c`): after a shot, a step across the line to the
   player, "mostly the other way from last time, now and then the same way
   again", only onto open floor still in sight of the player.
3. **Cover** (`2a2590c`): below `retreat_below` of its health, an enemy
   runs once "for the nearest spot it can reach out of the player's sight,
   not towards them, and hides there for four seconds".
4. **Spreading out** (`2a2590c`): crowd costs on other enemies' routes, an
   approach spot "on a side of its own, turned as far round from the others
   engaged as it can be", and never stopping in a doorway.
5. **Turns** (`2a2590c`): "no more of them shoot (or bite) at once than the
   difficulty lets, 2 on easy, 3 on normal and 4 on hard".

## C++ techniques used

- [Ranges algorithms](../techniques/ranges.md): ranking candidate bearings
  with `std::ranges::sort(choices, std::greater{})`; counting attackers with
  `std::ranges::count_if`.
- `std::array` of candidate turns (`constexpr`) and scores: no allocation
  per decision.

## Key code

- [The tactics branch of `WalkState::Update`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/State/src/enemy_state.cpp#L168-L250)
- [`Scene::MayAttack`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/scene.cpp#L237-L244)
- `Enemy::ApproachSpot`, `BackOffSpot`, `PlanSidestep`, `FindCover` in
  [`enemy.cpp`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Characters/src/enemy.cpp)

## Pitfalls

- The rules interact; `tests/tactics_test.cpp` pins them one at a time in
  small maps (an enemy keeps its range, steps aside, retreats, a group
  spreads out, a doorway stays clear).
- With enemies that never miss, every rule that keeps them from shooting
  at once is also a balance rule.

## What I'd change

- Flanking that uses the level's geometry (a second route to the player's
  room) rather than bearings round the player.
