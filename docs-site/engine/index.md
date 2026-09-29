# Subsystems

Each page in this section covers one subsystem of the engine, always in the
same order:

1. **Purpose**: what the subsystem is for.
2. **Concepts**: the theory it rests on, with diagrams and formulas.
3. **How it is implemented here**: the design, in words.
4. **Key code**: real excerpts, each linked to its lines on GitHub at a
   fixed commit.
5. **Design decisions and trade-offs**: what was chosen, and what it costs.
6. **Pitfalls**: what can go wrong, including problems found while writing
   these pages.
7. **Possible improvements**.

| Page | Subsystem | Code |
| --- | --- | --- |
| [Main loop and timing](main-loop.md) | Game states, frames, fixed ticks, interpolation, transitions | `Core/game`, `TimeManager` |
| [Raycasting and the camera](raycasting.md) | DDA through the grid, doors, sliding walls, projecting objects | `Camera` |
| [The 3D renderer](renderer.md) | The render queue: wall strips, sprites, decals, sky, weapon, HUD; the 2D view and the map | `Graphics` |
| [The map](map.md) | Cells, doors, push walls, the exit; map files | `GameMap` |
| [Collision and movement](collision.md) | Square bodies against walls, round bodies against each other | `CollisionManager`, `Player::Move`, `Enemy::Move` |
| [Game objects and the scene](entities.md) | `IGameObject`, the level's object list, pools, pickups, effects, projectiles | `GameObjects`, `Core/scene` |
| [The player](player.md) | Health, weapons carried, keys, pickups, the kick, dying | `Characters/player` |
| [Enemy AI](ai.md) | Perception, hearing, the state machine, tactics, taking turns | `Characters/enemy`, `State/enemy_state` |
| [Pathfinding](navigation.md) | Weighted A* on a fine grid, crowding, steering round lamps | `NavigationManager` |
| [Weapons and combat](combat.md) | Weapon states, hitscan shots, pellets, hit zones, masks, projectiles, damage | `Strike`, `State/weapon_state`, `ShootingManager` |
| [Input](input.md) | From SDL events to `PlayerCommand`; view angles; mouse on the web | `Core/game`, `Characters/player_command`, `view_angles` |
| [Audio](audio.md) | SDL_mixer channels, music, the spatial mixer | `SoundManager` |
| [Textures and animation](assets.md) | The texture manifest, clips seen from 8 sides, masks; the art pipeline | `TextureManager`, `Animation`, `scripts/` |
| [Levels and content data](levels.md) | `config.json`, level files, the SAX reader, the level generator, the campaign | `Core/level_data`, `Core/scene_loader`, `scripts/make_levels.py` |
| [Memory](memory.md) | The level arena, object pools, zero allocations after startup | `Allocators`, `app/allocation_counter.cpp` |
| [UI, menus and HUD](ui.md) | The immediate-mode toolkit, menus, story pages, text without allocating | `UI`, `Graphics/renderer_menu` |
| [Settings and saved games](persistence.md) | Records in `localStorage` or a file, the save format | `Settings` |
| [Profiling and testing](profiling.md) | Sections, the benchmark, the soak session, unit tests, CI gates | `Profiler`, `tests/`, `benchmarks/` |
| [Platform layer](platform.md) | SDL2 and its satellite libraries, native and web builds | `Graphics/renderer_interface`, `cmake/` |
