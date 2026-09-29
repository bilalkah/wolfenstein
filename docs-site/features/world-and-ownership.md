# A World that owns the game

| | |
| --- | --- |
| **When** | 23 to 25 September 2026 |
| **Commits** | `39846c6` Fix the shared_ptr ownership cycle in the state machines, `1a4f205` Apply the Rule of Zero and make copy/move semantics explicit, `8114156` Give every data member a default initialiser, `c9fb49e` Give each level a single owner and fold its singletons into it, `3ff2a23` Add a World that owns the simulation, and retire the global singletons |
| **Code today** | [Ownership and lifetimes](../architecture/ownership.md) |

## Problem

The 2024 engine shared everything: "The Scene was co-owned through
`shared_ptr` by up to eight holders (the game, both renderers, the camera
and four singletons that were never destroyed, so the last level lived
until exit), and Scene -> Player -> Camera2D -> Scene formed a reference
cycle" (`c9fb49e`). The state machines leaked every enemy and weapon
through another cycle (`39846c6`). Textures, sounds, the clock and the
level loader were singletons "never destroyed, so their textures and audio
were never released, and nothing ordered their shutdown against
`SDL_Quit`" (`3ff2a23`).

## Constraints

- Keep the game working through each step.
- Make teardown order explicit, since SDL objects must go before
  `SDL_Quit`.
- Let tests (and a future server) build a simulation without a window.

## Approach

1. **Break the cycle** (`39846c6`): a state holds a non-owning pointer to
   its owner, "which cannot dangle because the owner outlives every state
   it owns". It also fixed a state replacing itself from inside its own
   `Update`, "destroying the object whose member function was still
   running".
2. **Explicit special members** (`1a4f205`): Rule of Zero for ordinary
   types, protected copies for interfaces, deleted copies for owners.
3. **Stateless services become functions** (`c9fb49e`): "Line of sight,
   wall collision and shooting held no state of their own: they are now
   functions of what they read".
4. **A World** (`3ff2a23`): it owns the loader and configuration, the
   sound, the player and the current level; the `Game` owns one `World`
   next to the presentation. "Members are declared in dependency order,
   so teardown is explicit."

## C++ techniques used

- [Special members and pinned types](../techniques/special-members.md).
- [RAII](../techniques/raii.md) and declaration order as teardown order.
- `std::optional<T>` as a place to build a pinned object in (`player_`,
  `scene_`).

## Key code

- [The `Game`'s members, in dependency order](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/include/Core/game.h#L172-L181)
- [The `World`'s members](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/include/Core/world.h#L130-L135)
- [`State<T>` and `StateMachine<S>`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/State/include/State/state.h)

## Pitfalls

- Views that borrow the scene must be re-pointed after every level change
  (`Game::ShowLevel`); forgetting one would leave it pointing at a
  destroyed scene. AddressSanitizer in CI catches such a use.

## What I'd change

- Give views access to the current level through the `World` each frame
  instead of a stored pointer, removing the re-pointing step.
