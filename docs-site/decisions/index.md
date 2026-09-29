# Decision records

A decision record keeps one choice with its context, the options that were
weighed and what followed from it, so that a later reader (including the
author, a year on) knows why the code is the way it is before changing it.

The records below are for the choices that shaped the whole engine; they
were reconstructed from the commit messages, which state the reasons
directly from September 2026 on. Smaller choices are kept with the code
they concern, in the **Design decisions and trade-offs** section of each
[engine page](../engine/index.md). New records follow the
[template](template.md).

| # | Decision | Date | Status |
| --- | --- | --- | --- |
| 1 | [Clang and libc++ on every platform](#1-clang-and-libc-on-every-platform) | 2026-09-22 | Accepted |
| 2 | [A fixed timestep driven by commands](#2-a-fixed-timestep-driven-by-commands) | 2026-09-25 | Accepted |
| 3 | [One owner for the game: the World](#3-one-owner-for-the-game-the-world) | 2026-09-25 | Accepted |
| 4 | [A memory arena per level, pools per type](#4-a-memory-arena-per-level-pools-per-type) | 2026-09-25 | Accepted |
| 5 | [Pathfinding in the engine, not a submodule](#5-pathfinding-in-the-engine-not-a-submodule) | 2026-09-24 | Accepted |
| 6 | [Art and sound from Freedoom](#6-art-and-sound-from-freedoom) | 2026-09-28 | Accepted |
| 7 | [The page caches the game in IndexedDB](#7-the-page-caches-the-game-in-indexeddb) | 2026-09-25 | Accepted |
| 8 | [SDL_Renderer and the painter's algorithm](#8-sdl_renderer-and-the-painters-algorithm) | 2024 | Accepted |
| 9 | [A single-threaded web build](#9-a-single-threaded-web-build) | 2026-09-21 | Accepted |
| 10 | [This site's sources in `docs-site/`](#10-this-sites-sources-in-docs-site) | 2026-09-29 | Accepted |

## 1. Clang and libc++ on every platform

**Context.** The engine moved to C++23 for `std::expected`, `std::mdspan`
and the ranges library. The web build uses Emscripten
(Clang with libc++) and macOS uses Apple's Clang with libc++; Linux
defaulted to GCC with libstdc++.

**Decision** (`a2929f3`). "Probing every target showed std::mdspan is
missing even in GCC 15, so native builds now use Clang with libc++, the
same standard library as the Emscripten and macOS builds: one C++23
feature set and identical container behaviour on web, desktop and a future
Linux server."

**Consequences.** One compiler's warnings, one `clang-tidy`, one set of
container behaviours to reason about. The Linux toolchain comes from LLVM's
packages (`scripts/install_deps.sh`, or the dev container), not the
distribution's default compiler.

## 2. A fixed timestep driven by commands

**Context.** "The simulation read the keyboard and mouse itself, resolved
shots from the ray the camera cast while drawing, and stepped by the
variable frame time, so the same play gave different results at different
frame rates and could not be scripted, replayed or run without a view"
(`2beddb5`).

**Decision.** Input becomes a `PlayerCommand` per frame; the simulation
runs in fixed 1/60 s ticks, and the view is drawn between the last two
ticks by interpolation. Shots are resolved from the game state, not the
camera.

**Consequences.** Two worlds fed the same commands end identical, which
the tests check; the simulation runs headless in tests. Every moving
thing keeps a previous and a current pose. See
[Main loop and timing](../engine/main-loop.md) and
[A deterministic simulation](../features/deterministic-simulation.md).

## 3. One owner for the game: the World

**Context.** "The textures, sound, clock and level loader were singletons
that were never destroyed, so their textures and audio were never
released, and nothing ordered their shutdown against `SDL_Quit`"
(`3ff2a23`).

**Decision.** A `World` owns the simulation (configuration, levels, sound,
player, current level); `Game` owns one `World` next to the presentation,
which borrows from it. Members are declared in dependency order, so the
destructor order is the teardown order.

**Consequences.** No globals; lifetimes are visible in the class
definitions; tests build as many worlds as they like. See
[Ownership and lifetimes](../architecture/ownership.md).

## 4. A memory arena per level, pools per type

**Context.** The frame allocated hundreds of times (299 allocations per
frame before `f786c53`), and a level's objects were scattered over the
heap.

**Decision** (`1e1bbc5`, `8ffbef1`). Each level lives in one
`MonotonicArena` sized from the level before it is built; enemies and
objects are built in fixed pools with generational handles. Allocation
after startup is counted and fails the tests and the soak run.

**Consequences.** Zero allocations per frame, enforced in CI; a level is
freed in one step. Budgets must be computed (`MemoryFor`), and running
out is a hard error rather than a silent growth. See
[Memory](../engine/memory.md).

## 5. Pathfinding in the engine, not a submodule

**Context.** Enemies used the author's
[path-planning](https://github.com/bilalkah/path-planning) project as a
submodule, which made 250 of the 299 allocations per frame.

**Decision** (`f786c53`). A weighted A* inside the engine, on flat
per-level arrays with generation stamps, keeping the same behaviour
(unit steps, \(f = 0.4g + 0.6h\)).

**Consequences.** Zero allocations per query and one less submodule to
check out. See [Pathfinding](../engine/navigation.md).

## 6. Art and sound from Freedoom

**Context.** "Much of the art they replace was Doom's own, which cannot
be shipped" (`9b20b84`).

**Decision.** Enemies, weapons, pickups, walls, doors, the sky, the HUD
and most sounds come from Freedoom (BSD licence), imported by
`scripts/import_freedoom.py`; music followed in `9b20b84`. What is the
game's own is drawn by `scripts/make_art.py`.

**Consequences.** The game can be published, including in the browser.
The older art is still in the repository's history (see
[Freedoom art and music](../features/freedoom-art-and-music.md)).

## 7. The page caches the game in IndexedDB

**Context.** The game is nearly 19 MB (about 15 MB compressed),
downloaded again on every visit.

**Decision** (`711f255`). The page keeps the wasm and data files in
IndexedDB and reuses them while a `HEAD` request shows the server's copy
unchanged. "The Cache API needs https, and the game is also played from
http:// LAN addresses."

**Consequences.** A reload starts in under a second; the loader is the
page's own code, which has to cope with each server's handling of ranges
and compression (`73aaf65`). See
[Downloading and caching](../web/loading-and-caching.md).

## 8. SDL_Renderer and the painter's algorithm

**Context.** A raycaster draws the walls as columns and everything else
as flat pictures; it runs on macOS, Linux and in browsers.

**Decision.** Draw through SDL's 2D renderer (Metal, OpenGL, WebGL
underneath) with textured quads, sorted far to near, and no depth buffer.

**Consequences.** No shaders to port and one code path everywhere; less
control over batching. See [The 3D renderer](../engine/renderer.md).

!!! todo "Bilal: explain why"
    The history does not record why SDL_Renderer was chosen in 2024 over
    OpenGL or a software framebuffer. A guess: it was the quickest way to
    get textured columns on screen on macOS, and it later made the
    WebGL port free.

## 9. A single-threaded web build

**Context.** WebAssembly threads need `SharedArrayBuffer`, which browsers
enable only for cross-origin isolated pages; GitHub Pages cannot send the
headers that isolate a page.

**Decision.** The web build uses no threads. The game has none of its own
(the threaded raycaster was removed in `8ffbef1`); audio is mixed on the
main thread.

**Consequences.** The game runs from any static host. See
[Limitations](../web/limitations.md).

## 10. This site's sources in `docs-site/`

**Context.** MkDocs defaults to `docs/`, which in this repository is
git-ignored and holds local notes and benchmark records.

**Decision.** The site's sources are in `docs-site/` (`docs_dir` in
`mkdocs.yml`), and the web build is copied into `docs-site/play/` by CI
only.

**Consequences.** Local notes cannot leak into the published site, and
the site's sources are versioned with the code they describe.
