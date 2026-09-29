# Content as data

| | |
| --- | --- |
| **When** | 25 September 2026 |
| **Commits** | `1a8663e` Define textures, weapons and enemy types as data, `bd577fd` (SAX level reader), `506c3ae` Merge pull request #26 (data-driven) |
| **Code today** | [Levels and content data](../engine/levels.md), [Textures and animation](../engine/assets.md) |

## Problem

"Adding a weapon, an enemy type or a texture meant editing C++: about 250
LoadTexture lines, magic texture ids (sky 0, walls 1-5, crosshair 6 ...),
weapon stats in weapon.cpp, damage falloff chosen by comparing the
weapon's name, the menu's weapon list, and each enemy type's weapon and AI
tuning in lookup functions" (`1a8663e`).

## Constraints

- Keep every existing behaviour while moving it to data.
- Loading must validate: a mistake in data should fail at startup, with a
  message naming the file and field.
- No per-frame cost: names resolved once, ids used after.

## Approach

- **`assets/textures.json`** names every image: named textures, the wall
  texture for each map cell, animation clips as lists of frames. "An image
  used by several clips loads once (207 images instead of 214 loads)."
- **`assets/levels/config.json`** holds the player's stats, weapons (with
  their falloff as a field, not a name comparison), enemy types with their
  weapons and AI tuning, pickups, lamps, difficulties, and later the
  campaign and the story.
- **Typed structs, not a JSON tree, at run time**: the config is parsed
  into `GameConfig`, `WeaponConfig`, `EnemyConfig` and friends; level files
  are streamed with a SAX reader straight into `LevelData` (`bd577fd`:
  "loading a level makes 20 heap allocations natively instead of 176").
- **Errors as values**: parsers return `std::expected`, naming the file
  and what is wrong.

## C++ techniques used

- [Errors as values with `std::expected`](../techniques/expected.md)
- [Streaming JSON with SAX](../techniques/sax-parsing.md)
- [Heterogeneous lookup](../techniques/heterogeneous-lookup.md) for maps
  keyed by type name

## Key code

- [`GameConfig` and the config structs](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/include/Core/level_data.h)
- [`level_data.cpp`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/src/level_data.cpp):
  the config parser and the level reader.
- [`assets/levels/config.json`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/assets/levels/config.json)

## Pitfalls

- Indices into data (weapon slots, pickups in a level) are stored in saved
  games, so data order matters; see
  [Settings and saved games](../engine/persistence.md).
- A name used in code (`"sky"`, `"crosshair"`) must exist in the manifest:
  the lookup exits at startup if not, which keeps the failure early.

## What I'd change

- A JSON Schema for both files, checked in CI and usable by editors.
