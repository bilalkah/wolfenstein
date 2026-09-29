# A raycasting engine, from the inside

This site documents **Wolfenstein**: a first-person shooter in the style of
*Wolfenstein 3D* and *Doom*, and the small engine under it, written in C++23
with SDL2. It runs natively and, compiled to WebAssembly, in the browser.

<a class="md-button md-button--primary" href="play/">Play it in your browser</a>
<a class="md-button" href="https://github.com/bilalkah/wolfenstein">Source on GitHub</a>

The game opens on a page of its own. The first visit downloads about 15 MB;
later ones start from the browser's copy. Click the game to capture the
mouse; Esc releases it and pauses.

<figure markdown="span">
  ![The barracks courtyard: a soldier comes round a pillar](assets/screenshots/combat-barracks.png){ width="640" }
  <figcaption>The barracks: textured walls, sprites, the HUD and the map in the corner.</figcaption>
</figure>

## What it is

- A **raycaster**: the world is a grid of wall cells, and every frame the
  engine casts one ray per two screen columns through that grid (the DDA
  algorithm) to find how far away the wall in that direction is. Near walls
  are drawn tall, far walls short. Enemies, lamps, pickups and effects are
  flat pictures (billboards) placed among the wall columns.
  See [Raycasting and the camera](engine/raycasting.md) and
  [The 3D renderer](engine/renderer.md).
- A **deterministic simulation** that advances in fixed 1/60 s ticks, driven
  only by `PlayerCommand` values, while the renderer draws as often as the
  display allows and interpolates between ticks.
  See [Main loop and timing](engine/main-loop.md).
- A **campaign** of fifteen levels in three chapters, generated from room
  layouts by a script, with enemies that patrol, hear gunfire, take cover
  and take turns to shoot, keys and locked doors, secret walls, pickups,
  story pages and intel pinned to walls.
- An exercise in **engineering discipline**: after startup the game makes
  no heap allocation at all, on native builds and in the browser, and CI
  fails if it does. Levels live in one arena; objects in fixed pools.
  See [Memory](engine/memory.md).

## Controls

| Action | Keys |
| --- | --- |
| Move | W A S D |
| Turn | Mouse, or Left / Right arrows |
| Look up and down | Mouse |
| Fire | Left click (hold for automatic weapons) |
| Reload | R |
| Weapons | Number keys 1 to 8, or the mouse wheel |
| Open a door, push a secret wall, throw the exit switch, read intel again | E, or Space |
| Map, small or large | M |
| Pause | Esc |
| Menus, story pages, briefings | Arrow keys, Enter, Esc (Space, E or a click go on) |

With `?debug` in the address (for example `play/?debug`), **P** switches
to the developer's top-down view: the grid, every ray, the enemies and the
paths they plan. `?benchmark=2000` and `?soak` run the scripted
[benchmark and soak sessions](engine/profiling.md) instead of the game.

## How to read this site

The site is written for two readers: the author, learning the engine in
depth, and anyone who wants to see how a game engine like this one fits
together. Everything in it is grounded in the code at a fixed commit; code
excerpts link to the exact lines on GitHub.

1. Start with **[The big picture](architecture/index.md)**: the code map,
   the modules and how they depend on each other. Then follow
   **[one frame from input to pixels](architecture/frame-lifecycle.md)**:
   it touches almost every subsystem once, in order.
2. **[Engine](engine/index.md)** has a page per subsystem, each laid out the
   same way: *purpose*, the *concepts* behind it (the maths of raycasting,
   A*, a fixed timestep), *how it is implemented here*, *key code*, *design
   decisions and trade-offs*, *pitfalls*, and *possible improvements*.
3. **[Features](features/index.md)** tells how the game grew, commit by
   commit: each feature's problem, constraints and approach, and what
   could change.
4. **[C++ techniques](techniques/index.md)** explains each language
   technique the code uses, in general first and then where and why it
   appears here.
5. **[The web build](web/index.md)** covers what it takes to run the same
   code in a browser: Emscripten, the main loop, pointer lock, audio,
   downloading and caching.

Click a diagram or a screenshot to see it full-screen. In a diagram, scroll
or pinch to zoom and drag to move around; Esc closes it.
