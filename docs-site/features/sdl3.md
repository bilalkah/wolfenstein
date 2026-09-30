# Moving to SDL 3

| | |
| --- | --- |
| **When** | 29 September 2026 |
| **Commits** | `e9f5599` Move to SDL 3, built from source for every platform, `bf24807` Keep headless runs off OpenGL |
| **Code today** | [Platform layer](../engine/platform.md), [Audio](../engine/audio.md) |

## Problem

The engine ran on SDL 2, from system packages natively and from
Emscripten's ports in the browser. SDL 3 is where SDL's development goes
on; its mixer and renderer changed a great deal, and the move was due
before multiplayer added more code on top.

## Constraints

- The same library versions natively and in the browser: "Emscripten has
  no SDL3_image or SDL3_mixer port and Ubuntu 26.04 ships SDL 3.4.2
  without SDL3_mixer".
- No allocation once a game runs, sound included.
- The web build must stay small and paced by the display.

## Approach

1. **Built from source, everywhere.** "SDL 3.4.16, SDL3_image 3.4.6,
   SDL3_ttf 3.2.2 (with FreeType 2.14.3) and SDL3_mixer 3.2.4 are
   downloaded and built with the game, pinned by checksum, the same way
   natively and in the browser." Static, with only what the game uses
   (PNG through stb_image, MP3 through dr_mp3; no GPU API, gamepads,
   camera or dialogs): "the web build's wasm is a quarter smaller (2.4 MB,
   was 3.3 MB)". Optimised in debug builds too, "as system packages are:
   the tests draw through SDL's software renderer, which took six times as
   long unoptimised".
2. **SDL 3's API.** Float rectangles, the new event names, keyboard state
   as bools, relative mouse mode per window, a centred window made from
   properties; the default scale mode set to nearest "before any texture
   is made, so the pixel art stays sharp".
3. **A logical presentation.** "In the browser SDL 3 makes the window the
   size the page gives the canvas", so the views draw at the configured
   size and SDL scales the picture to the window, letterboxed; menu
   clicks are turned into the picture's coordinates.
4. **Pacing by vsync in the browser.** "SDL 3 also paces the page's main
   loop by the renderer's vsync, on setTimeout when it is off: vsync is
   asked for in the browser, so the loop runs on requestAnimationFrame as
   before (58 frames a second in headless Chromium, 146 without)."
5. **The game mixes the sound effects itself.** "SDL3_mixer allocates
   whenever one of its tracks is given a new sound, and a track's buffers
   are set up the first time it plays. So every sound effect is now mixed
   by the SpatialMixer, … in float; each music track has an SDL_mixer
   track of its own; and the game opens the device itself and fills it
   with SDL_mixer's music (MIX_Generate) and the effects over it."
6. **Headless off OpenGL** (`bf24807`): SDL 3 on Linux presents even the
   software renderer through OpenGL when it can; headless, that loaded
   Mesa's llvmpipe, "whose shader compiler (LLVM's JIT) crashed under
   AddressSanitizer on CI's x86 runner". CI and `dev.sh` set
   `SDL_FRAMEBUFFER_ACCELERATION=0`.

## C++ techniques used

- CMake `FetchContent` with checksums, and an interface target per
  library, so modules link `wolfenstein::sdl3_mixer` and never care where
  it came from.
- A callback the audio device calls (`SoundManager::Feed`), filling a
  fixed buffer: no allocation on the audio thread.

## Key code

- `cmake/Dependencies.cmake`: the pinned libraries and their options.
- `RendererContext` (`src/Graphics/src/renderer_interface.cpp`): the
  window from properties, the logical presentation, vsync in the browser.
- `SoundManager::Open` and `SoundManager::Feed`
  (`src/SoundManager/src/sound_manager.cpp`): the mixer, the tracks, the
  device stream.

## Pitfalls

- **Warming up at startup.** Each music track is played a moment,
  unheard, and the rolled view a dead player sees is drawn once, so
  neither sets itself up (allocating) during play.
- **A first build is longer.** SDL and its libraries compile with the
  game (a few minutes), natively and for the web.

## What I'd change

- Move the web audio to an `AudioWorklet` when SDL does; it still uses a
  `ScriptProcessorNode` on the main thread.
