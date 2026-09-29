# Pickups and drops

| | |
| --- | --- |
| **When** | 26 to 29 September 2026 |
| **Commits** | `59288f3` Add health and ammo pickups, with a limited ammo reserve, `28b59ee` Let enemies drop what they carry, `d41e706` Drop ammunition and health by chance |
| **Code today** | [The player](../engine/player.md#taking-pickups), [Game objects and the scene](../engine/entities.md#pickups-and-drops), [Levels and content data](../engine/levels.md#populating-per-level) |

## Problem

Make health and ammunition scarce resources: health that does not
regenerate, rounds that run out, and supplies found in levels and on the
fallen, in amounts that depend on the difficulty.

## Constraints

- Taking or dropping a pickup allocates nothing.
- A saved game restores which pickups were taken and what enemies carry.
- The same game plays the same way (drops from a seed).

## Approach

- **Pickups** (`59288f3`): medkits, large medkits and ammo boxes, defined
  in `config.json`; "a medkit at full health or a box with a full reserve
  stays where it lies". Weapons carry a reserve besides the magazine;
  "Health no longer regenerates: medkits are the way back."
- **Drops** (`28b59ee`): "Each drop is a pickup made with the level, after
  its own, hidden until its enemy dies, so a death allocates nothing and a
  saved game keeps drops as it keeps pickups."
- **Chances** (`d41e706`): "a soldier carries a clip three times in four and
  a medkit one time in seven ... a cyber demon always leaves a large medkit
  and a box of ammunition. The difficulty's supplies scale the chances."
  "What each enemy carries is rolled as its level is made, from the game's
  seed: a new game rolls afresh, and a saved game keeps its seed."

## C++ techniques used

- Object pools and hidden-until-needed objects instead of creating
  objects mid-level ([Game objects and the scene](../engine/entities.md)).
- A counter-based hash (FNV-1a over the map's name, then SplitMix64) as a
  stateless random function of (seed, level, enemy, drop).

## Key code

- [`Player::TryPickUp`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/player.cpp#L196-L245)
- [`DropRoll`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene_loader.cpp#L20-L34)
- [Drops made at load](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene_loader.cpp#L204-L230)

## Pitfalls

- Drops count against the 64-pickup limit of a saved game's bit set, every
  possible drop included; the level-design test checks it.
- The level's supplies in its results are its own, not the drops.

## What I'd change

- A drop table per difficulty rather than one scale factor, if balancing
  needs finer control.
