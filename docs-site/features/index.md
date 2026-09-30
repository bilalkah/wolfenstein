# How the game grew

The repository's history (about 150 commits) falls into four periods:

- **2024, the raycaster** (February to December): the first 2D and 3D
  views, textures, sprites, animation, pathfinding through a submodule,
  weapons with states, menus, sound. Commit messages are short; the code
  of that time survives in shape rather than in detail.
- **21 to 25 September 2026, the engine overhaul**: WebAssembly, C++23,
  target-based CMake with Clang and libc++, CI with sanitizers, formatting
  and `clang-tidy`, a benchmark, zero allocations per frame, a `World` with
  single ownership, a deterministic simulation, content as data. Commit
  messages from here on explain the problem, the change and often the
  measurements.
- **26 to 29 September 2026, the game**: doors, keys, secrets, objectives,
  pickups, difficulties, saves, a campaign, an arsenal, Freedoom art and
  music, enemies that hear and use tactics, positional sound, a story and
  intel, fifteen levels.
- **29 and 30 September 2026, SDL 3 and multiplayer**: the move to SDL 3,
  built from source for every platform, and a deathmatch for up to eight
  players on a server, in the browser and the native game together.

Each page below follows the same outline: **Problem**, **Constraints**,
**Approach**, **C++ techniques used**, **Key code**, **Pitfalls**, and
**What I'd change**. Quotations are from the commit messages.

## Timeline

| Feature | When | Key commits |
| --- | --- | --- |
| [The first raycaster](raycaster-origins.md) | 2024 | `ef50b73`, `4d25943`, `1f6a29f` |
| [Sprites and animation](sprites-and-animation.md) | 2024, 2026 | `37597fd`, `31770ed`, `9b20b84`, `e024275` |
| [Weapons](weapons.md) | 2024, 2026 | `95276c9`, `f79930a`, `0bf0e6e`, `4f55a94` |
| [Enemy pathfinding](enemy-pathfinding.md) | 2024, 2026 | `cfeddd5`, `f786c53`, `7524277` |
| [Menus and settings](menus-and-settings.md) | 2026-09-21 | `1fd5f0b`, `8d46824`, `7524277` |
| [The toolchain and CI](toolchain-and-ci.md) | 2026-09-21 to 25 | `a2929f3`, `a220068`, `3b1d817`, `5d62b2a` |
| [Playing in the browser](webassembly.md) | 2026-09-21 to 29 | `903f2a3`, `711f255`, `de89d1c`, `73aaf65` |
| [Zero allocations per frame](zero-allocations.md) | 2026-09-24 to 25 | `3d0c8ab`, `3faf914`, `1e1bbc5`, `c2ac155` |
| [A World that owns the game](world-and-ownership.md) | 2026-09-23 to 25 | `39846c6`, `c9fb49e`, `3ff2a23` |
| [A deterministic simulation](deterministic-simulation.md) | 2026-09-25 to 29 | `2beddb5`, `7524277`, `d41e706` |
| [Content as data](data-driven-content.md) | 2026-09-25 | `1a8663e`, `bd577fd` |
| [The campaign](campaign.md) | 2026-09-25 to 29 | `6ecde29`, `1c084af`, `64ee770`, `5880c6e` |
| [Doors, keys and secrets](doors-keys-secrets.md) | 2026-09-26 | `7d8bdc3`, `d029813`, `e0fb909` |
| [Pickups and drops](pickups-and-drops.md) | 2026-09-26 to 29 | `59288f3`, `28b59ee`, `d41e706` |
| [The map of the level](map-and-exploration.md) | 2026-09-26 | `c406a58` |
| [Difficulty and saved games](difficulty-and-saves.md) | 2026-09-26 | `113f9ca`, `568f834`, `c57f38b` |
| [Bodies that collide](collision-and-physics.md) | 2026-09-26 to 29 | `ea93fbe`, `6f2e0da`, `e8e9914`, `d41e706` |
| [Where shots land](hit-detection.md) | 2026-09-27 to 28 | `20c414c`, `e024275`, `7524277` |
| [Looking up and down](looking-up-and-down.md) | 2026-09-28 to 29 | `de89d1c`, `de89d1c`, `d41e706` |
| [Freedoom art and music](freedoom-art-and-music.md) | 2026-09-28 | `9b20b84`, `9b20b84` |
| [Enemy senses](enemy-senses.md) | 2026-09-28 | `28b59ee`, `9b20b84`, `9b20b84` |
| [Combat tactics](combat-tactics.md) | 2026-09-28 | `2a2590c`, `2a2590c`, `2a2590c`, `2a2590c`, `2a2590c` |
| [Rockets and plasma](projectiles.md) | 2026-09-28 | `4f55a94` |
| [Dying and hurting](death-and-feedback.md) | 2026-09-28 | `64ee770`, `28b59ee` |
| [Hearing where sounds are](positional-audio.md) | 2026-09-29 | `d41e706` |
| [Story and intel](story-and-intel.md) | 2026-09-26 to 29 | `0bf1592`, `5880c6e`, `5880c6e` |
| [Moving to SDL 3](sdl3.md) | 2026-09-29 | `e9f5599`, `bf24807` |
| [Multiplayer](multiplayer.md) | 2026-09-30 | `5bca75b`, `a6cdb55`, `728a115`, `1ed5290`, `ae5cbde`, `60a1902` |

!!! note "Squashed history"
    The commits of 28 and 29 September 2026 were squashed before they were
    published: a hash cited for one of those days names the commit its
    change went into, which may hold several of the changes listed. Each
    squashed commit's message keeps the messages of the commits it joined,
    so the quotations on these pages can still be found there.
