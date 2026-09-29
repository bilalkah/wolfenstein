# Platform layer

## Purpose

The engine runs natively (on Linux, where the dev container and CI build
it) and in browsers, from one code base. The platform layer is what makes that possible: SDL2 and its three
satellite libraries for windows, drawing, fonts, images and sound; CMake
targets that pick system packages natively and Emscripten ports on the
web; and a handful of `#ifdef __EMSCRIPTEN__` blocks for what genuinely
differs.

Code: `src/Graphics/src/renderer_interface.cpp` (`RendererContext`),
`cmake/Dependencies.cmake`, `cmake/ProjectOptions.cmake`,
`CMakePresets.json`, `docker/dev.Dockerfile`, `scripts/install_deps.sh`.

## Concepts

### A portability layer

SDL (Simple DirectMedia Layer) hides the operating system and graphics API
behind one C API: create a window, poll events, draw textured rectangles
and triangles with `SDL_Renderer` (backed by Metal, Direct3D, OpenGL or,
in a browser, WebGL), open an audio device. Its satellite libraries add
image decoding (SDL_image), TrueType text (SDL_ttf) and mixing and music
(SDL_mixer). Emscripten ships all four as **ports**: selecting them is a
compiler flag, and they are built from source for WebAssembly on first use.

## How it is implemented here

### Dependencies as targets

`cmake/Dependencies.cmake` exposes `wolfenstein::sdl2`,
`wolfenstein::sdl2_image`, `wolfenstein::sdl2_ttf` and
`wolfenstein::sdl2_mixer`. Natively they wrap the system packages found by
`find_package`; on the web they are interface targets carrying the port
flags (`-sUSE_SDL=2`, `-sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS=png,jpg`,
`-sUSE_SDL_TTF=2`, `-sUSE_SDL_MIXER=2 -sSDL2_MIXER_FORMATS=mp3`). Modules
link the `wolfenstein::` names and never care which. nlohmann/json is
downloaded at a pinned version with a checksum; GoogleTest and Google
Benchmark likewise, for native builds only.

### Project-wide options

`cmake/ProjectOptions.cmake` defines one interface target,
`wolfenstein::options`, and a function that links it into every target
under `app/`, `src/` and `tests/` (and no downloaded dependency). It
carries:

- C++23;
- `RESOURCE_DIR`: the repository's `assets/` natively, `/assets/` on the
  web (the preloaded virtual file system);
- a strict warning set, and `-Werror` in the presets;
- sanitizers when `WOLFENSTEIN_SANITIZERS` is set (the `native-asan` preset
  uses AddressSanitizer and UndefinedBehaviorSanitizer);
- threads natively only: "Browsers only allow threads on cross-origin
  isolated pages, so the web build is single threaded".

### Presets and toolchains

| Preset | Compiler | Use |
| --- | --- | --- |
| `native-debug` | Clang, libc++ | Development, tests, the allocation gates |
| `native-release` | Clang, libc++ (`RelWithDebInfo`) | Playing, benchmarking |
| `native-asan` | Clang, libc++, ASan + UBSan | Tests under sanitizers |
| `web-release` | Emscripten (`$EMSDK`'s toolchain file) | The browser build |

Clang with libc++ everywhere, so the native builds use the same standard
library as the Emscripten and macOS builds ("libstdc++ lacks `<mdspan>`
even in GCC 15", `install_deps.sh` notes). The native toolchain also comes
as a container (`docker/dev.Dockerfile`, Ubuntu 26.04 with LLVM 21):
`scripts/dev.sh <command>` runs any command in it.

### The renderer context

`RendererContext` owns the SDL window, an accelerated `SDL_Renderer`, the
HUD font and the `TextureManager`. Besides creating them, its constructor
**warms the renderer up**: thousands of throwaway copies, one of each kind
of draw, and one geometry batch as large as the 2D view ever draws, so
SDL's command pool and vertex buffer reach their final size and the GPU
driver compiles its shaders at startup rather than in the first frames of
play (see [The 3D renderer](renderer.md#warming-up-at-startup)). It exits
the program with a message if SDL, the font or a texture cannot be
loaded: there is no game without them.

### What differs on the web

| Where | Native | Web |
| --- | --- | --- |
| `Game::Run` | `while (Tick()) {}` | `emscripten_set_main_loop_arg`: the browser calls `Tick` each animation frame |
| `Game::GameTick` | Sleeps to cap at 120 Hz | No sleep: `requestAnimationFrame` paces |
| `Game::CheckGameEvent` | Esc pauses | Losing pointer lock pauses (the browser eats Esc) |
| `Game::CanvasStretch` | 1 | The canvas's CSS width over its pixels |
| `storage.cpp` | Files in SDL's preferences directory | `localStorage`, through `EM_JS` |
| `allocation_counter.cpp` | Counts `operator new` | Counts `malloc` (and friends) |
| `renderer_menu.cpp` | A Quit button | No Quit ("a browser tab cannot close itself"); a hint about capturing the mouse |

Everything else, the whole simulation and renderer included, is the same
code. See [The web build](../web/index.md).

## Design decisions and trade-offs

- **SDL_Renderer over OpenGL.** No shaders to port to WebGL, and the same
  code runs on every backend; the cost is less control over batching.
- **One interface target for options.** Module `CMakeLists.txt` files stay
  free of boilerplate; the options apply everywhere automatically.
- **Pinned downloads with checksums.** Reproducible builds without
  vendoring third-party code.

## Pitfalls

- **`exit()` on missing resources.** `RendererContext` and the texture
  lookups end the program on failure, which is right for a game but means
  those paths cannot be unit-tested; tests use placeholder textures.
- **Static library cycles.** Some modules link each other both ways (see
  [The big picture](../architecture/index.md#the-module-dependency-graph));
  CMake copes, but the layering is not enforced by the build.

## Possible improvements

- A `native-release` build with link-time optimisation.
- Gamepad input and a fullscreen toggle through SDL.
