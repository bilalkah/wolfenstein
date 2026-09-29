# The campaign

| | |
| --- | --- |
| **When** | 25 to 29 September 2026 |
| **Commits** | `6ecde29` Play a campaign of four levels with no allocation after startup, `1c084af` Add level objectives and an exit switch that ends the level, `0bf1592` Brief the player before each level, `dc05231` Count kills on the HUD and show each level's results, `64ee770` Add three levels: the forge, the archives and the keep, `5880c6e` Lengthen the campaign to fifteen levels in three chapters |
| **Code today** | [Levels and content data](../engine/levels.md), `scripts/make_levels.py` |

## Problem

Turn a sequence of maps into a campaign: levels that build on each other,
each with a goal and a way out, a briefing before and results after, and a
difficulty that rises as the player finds better weapons.

## Constraints

- Levels generated from readable layouts, reproducibly.
- Every shipped level provably playable (keys before their locks, exits
  reachable).
- No allocation or hitch between levels.

## Approach

- **A campaign in config** (`6ecde29`): the levels in order, all prepared
  at startup and rebuilt in place in one arena.
- **Objectives and an exit** (`1c084af`): "An X in a map is the level's
  exit switch ... kill every enemy, or kill the enemies marked as targets
  ... Using the exit once they are done ends the level; before that it says
  'The mission is not done yet'."
- **Briefings and results** (`0bf1592`, `dc05231`): a few lines of story
  before each level; kills, supplies, secrets, time and exploration after.
- **More levels** (`64ee770`, `5880c6e`): seven, then fifteen levels in
  three chapters (The Valley, The Works, The Castle), with the guns spread
  through them: "the shotgun in the depot, the super shotgun in the relay,
  the plasma rifle in the laboratory".
- **The generator** (`scripts/make_levels.py`): rooms, corridors, locks,
  hidden rooms, placement by rules, and checks; and a C++ test over every
  shipped level (see [Levels and content data](../engine/levels.md#checking-the-shipped-levels)).

<figure markdown="span">
  ![A level's briefing](../assets/screenshots/briefing.png){ width="480" }
  <figcaption>The first level's briefing, with its objectives.</figcaption>
</figure>

## C++ techniques used

- `std::optional::emplace` to rebuild the `Scene` in place for each level.
- [Polymorphic memory resources](../techniques/pmr-arenas.md): one arena,
  rewound per level.
- [Non-owning views](../techniques/views.md) for briefings and objectives
  drawn from the config.

## Key code

- [`World::StartLevel`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/world.cpp#L240-L262)
- [The level generator](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/scripts/make_levels.py)
- [The level-design test](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/tests/level_design_test.cpp)

## Pitfalls

- "Firing at the last enemy as it fell took the alive count below zero,
  where it wrapped round and the level never ended" (`ad36583`): kills are
  counted once, by the killing shot.
- Regenerating levels reorders pickups; the save format number must follow.

## What I'd change

- A chapter select after a chapter is finished, for replaying.
