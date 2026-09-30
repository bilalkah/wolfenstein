# Problems solved

Each problem, how it was solved, and where the code is. The algorithms
named here are explained in [Algorithms](algorithms.md).

## Drawing a 3D view from a 2D map, fast enough for a browser

- **Raycasting**: one ray per two screen columns, walked through the grid
  cell by cell (DDA) to the first wall; the wall's height comes from its
  perpendicular distance.
- Every wall strip, sprite and decal becomes a textured quad in one
  **render queue**, sorted far to near and drawn by the GPU (painter's
  algorithm): no depth buffer, no pixel loop on the CPU.
- Doors and secret walls are handled where a ray meets them, not by
  changing the grid.

Code: `src/Camera/`, `src/Graphics/src/renderer_3d.cpp`.

## The same result at any frame rate, and a game that can be tested

- **Problem.** The simulation used to step by the frame's time and read the
  keyboard itself: results changed with the frame rate and could not be
  scripted.
- **Solution.** Input becomes a `PlayerCommand`; the simulation runs in
  **fixed 1/60 s ticks** from commands only, and frames are drawn
  interpolated between ticks. The view angle is turned every frame and
  carried by the command, so aiming has no lag.
- Two worlds fed the same commands end identical; tests, the benchmark
  and multiplayer all rely on it.

Code: `src/TimeManager/`, `src/Characters/include/Characters/player_command.h`, `view_angles.h`.

## No stutter from memory allocation

- **Problem.** Allocating during play costs time at random moments, more
  so in WebAssembly.
- **Solution.**
    - One **arena** per level, sized at startup for the largest level and
      reset between levels.
    - **Fixed pools** for enemies, lamps and pickups, handed out as
      generational handles.
    - Everything that fills during a level is reserved when it starts;
      what comes and goes (puffs, projectiles, voices, sound commands)
      lives in fixed arrays; text is formatted into stack buffers.
- **Enforced.** A replacement allocator counts every allocation; the
  benchmark and a scripted session through every screen fail CI on any
  allocation after startup, natively and in Chromium.

Code: `src/Allocators/`, `app/allocation_counter.cpp`.

## Enemies that behave well, and fight fair

- A **state machine** per enemy: idle, patrol, walk (hunt), attack, pain,
  retreat, death. States are members, so a transition allocates nothing.
- **Sight**: a line-of-sight ray to the player each tick, within the
  type's range. **Hearing**: a gunshot floods outward through open cells;
  enemies it reaches come hunting.
- **Tactics**: each type keeps a preferred distance, spreads round the
  player instead of queueing, sidesteps after firing, retreats to cover
  when badly hurt.
- **Taking turns**: only 2 to 4 enemies (by difficulty) fire at once.

Code: `src/Characters/src/enemy.cpp`, `src/State/`.

## Moving through corridors, round each other

- **Weighted A\*** on a grid of half-cells, so two enemies can pass in a
  one-cell corridor.
- Costs steer groups apart (another enemy's route costs extra, other
  enemies block), and keep clear of lamps.
- Only the next step is used, and paths are replanned every tick; a query
  allocates nothing and clears nothing (generation stamps).

Code: `src/NavigationManager/`.

## Bodies against walls and against each other

- Walls: a square body, tested on its **leading edge**, one axis at a
  time, so it slides along walls.
- Bodies: circles; a step that ends inside another is pushed out onto its
  edge. Enemies are not pushed out of each other (a crowd would jam a
  doorway); they ease apart instead.

Code: `src/CollisionManager/`, `Player::Move`, `Enemy::Move`.

## Shots that hit what you see

- **Hitscan**: a shot is tested against each target as a flat board facing
  the shooter, then against the **mask** of the picture shown, so a shot
  between a soldier's legs misses.
- **Hit zones**: head (double damage), body, legs (less).
- Shotguns fan their pellets across a spread; rockets and plasma fly in
  small steps and burst on what they meet, their blast hurting whoever is
  in reach and in sight.

Code: `src/ShootingManager/`, `src/Core/src/scene.cpp` (projectiles).

## Sounds from where they happen

- **Problem.** Every sound played centred and full, and SDL_mixer's own
  positioning allocates each time a sound starts.
- **Solution.** The game mixes every effect itself (40 voices): each sound
  from a place is panned and faded by distance and angle, and muffled
  through walls; SDL_mixer plays only the music. The game thread talks to
  the audio thread through a **lock-free ring**, so neither waits.

Code: `src/SoundManager/`.

## Content as data

- Weapons, enemies, pickups, difficulties and the campaign are in
  `config.json`; each level is a text grid plus a JSON file (read as a
  stream).
- Levels and arenas are generated from text layouts by scripts that check
  them (every cell reachable, doors in walls, spawn spacing).
- Adding a level, enemy or weapon needs no code.

Code: `src/Core/src/level_data.cpp`, `scripts/make_levels.py`, `scripts/make_arenas.py`.

## Saving, natively and in the browser

- Settings and saved games are small `key=value` records with a format
  number: a file in SDL's preferences directory natively, `localStorage` in
  the browser.
- Written into stack buffers, so saving during play allocates nothing; a
  save is taken automatically when nothing is fighting the player.

Code: `src/Settings/`.

## Running in the browser

- **The loop**: the browser calls one `Game::Tick` per animation frame
  (`emscripten_set_main_loop`); SDL 3 paces it by the renderer's vsync.
- **The mouse**: pointer lock with raw motion; the canvas's stretch is
  undone so the mouse turns as far at any size.
- **Sound**: resumed on the first click or key (browsers start audio
  suspended).
- **No threads**: the web build is single-threaded, so it runs from any
  static host.

Code: `src/Core/src/game.cpp`, `web/shell.html`.

## A 15 MB download that survives bad networks

- **Parallel, resumable pieces**: the page downloads the game in 1 MB byte
  ranges over 4 connections; a piece that stalls for 2 s is resumed on a
  new connection from the byte it reached.
- **Cached by version** in IndexedDB: the next visit starts from the local
  copy, and checks the server's size and date to know it is current.
- **Self-healing**: a copy that fails to start is cleared and the page
  reloads once.

Code: `web/shell.html`.

## Pixel art on any screen

- The game draws at a fixed size and SDL 3 scales the picture to the
  window, letterboxed, with nearest-pixel sampling so the art stays sharp.
  Clicks are mapped back into the picture's coordinates.

Code: `src/Graphics/src/renderer_interface.cpp`.

## Playing together

- An authoritative server, predicted movement, interpolated opponents,
  shots judged where the shooter saw the target, rooms, rejoining after a
  drop. See [Multiplayer](multiplayer.md).
