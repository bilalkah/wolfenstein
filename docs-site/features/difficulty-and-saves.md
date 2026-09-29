# Difficulty and saved games

| | |
| --- | --- |
| **When** | 26 to 29 September 2026 |
| **Commits** | `113f9ca` Add Easy, Normal and Hard difficulties, `568f834` Save the campaign as each level starts, and continue it from the menu, `e0c9c42` Choose the difficulty once, when a new game starts, `c57f38b` Save where the player is, whenever nothing is fighting them, and the save format bumps with later content (`5880c6e`, `5880c6e`) |
| **Code today** | [Settings and saved games](../engine/persistence.md) |

## Problem

Let different players find the right challenge, and let a campaign of
fifteen levels be played over several sessions.

## Constraints

- Difficulty as data: multipliers, not special cases in code.
- Saving must not allocate and must never happen mid-fight.
- A save from older content must not load as nonsense.

## Approach

- **Difficulties** (`113f9ca`, `e0c9c42`): `config.json` lists them with
  multipliers for enemy damage, enemy health and supplies (and later how
  many enemies may shoot at once); chosen once, when a new game starts.
  "The benchmark and the soak play Normal, so their runs stay comparable."
- **Saving levels** (`568f834`): the level, weapon, difficulty, health and
  rounds as each level starts; CONTINUE on the main menu. "Dying keeps the
  save for another try; winning the campaign clears it."
- **Saving the moment** (`c57f38b`): "a snapshot of the level as the player
  left it: where they stand and face, what they carry, which enemies they
  killed (lying where they fell), which pickups they took, what they
  explored and the level's clock", saved "every five seconds while it is
  quiet ... and on quitting to the menu when quiet; never in a fight".
- **A format number** bumped whenever the content changes what indices
  mean (format 4 today).

## C++ techniques used

- [Text without allocating](../techniques/allocation-free-text.md):
  `RecordWriter` with `std::to_chars`.
- `EM_JS` for `localStorage` (see [Storage](../web/storage.md)).
- `World::Capture` returns `std::optional<SavedGame>` (nothing outside the
  campaign).

## Key code

- [`Game::AutoSave`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/game.cpp#L227-L236)
- [`World::Capture` and `ContinueGame`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/world.cpp)
- [`SavedGame::kFormat`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Settings/include/Settings/saved_game.h#L24-L29)

## Pitfalls

- Bit sets of 64 per level for enemies, pickups, secrets and intel: a
  larger level would need a wider record.
- Continuing restores killed enemies "silently, straight to the end of
  their death", not replaying their fall.

## What I'd change

- Save slots, so several campaigns can run side by side.
