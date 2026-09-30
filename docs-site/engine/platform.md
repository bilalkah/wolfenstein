# Platform layer

## Purpose

The engine runs natively (on Linux, where the dev container and CI build
it, and on macOS) and in browsers, from one code base. The platform layer
is what makes that possible: SDL 3 and its three libraries for windows,
drawing, fonts, images and sound, built from source with the game at the
same versions everywhere; CMake targets that hide where each library
comes from; and a handful of `#ifdef __EMSCRIPTEN__` blocks for what
genuinely differs.

Code: `src/Graphics/src/renderer_interface.cpp` (`RendererContext`),
`cmake/Dependencies.cmake`, `cmake/ProjectOptions.cmake`,
`CMakePresets.json`, `docker/dev.Dockerfile`, `scripts/install_deps.sh`.

## Concepts

### A portability layer

SDL (Simple DirectMedia Layer) hides the operating system and graphics API
behind one C API: create a window, poll events, draw textured rectangles
and triangles with `SDL_Renderer` (backed by Metal, Direct3D, OpenGL or,
in a browser, WebGL), open an audio device. Its libraries add image
decoding (SDL3_image), TrueType text (SDL3_ttf, over FreeType) and mixing
and music (SDL3_mixer). All four compile for WebAssembly with Emscripten
as they do natively, so the game builds them from source the same way on
every platform.

## How it is implemented here

### Dependencies as targets

`cmake/Dependencies.cmake` downloads SDL 3.4.16, SDL3_image 3.4.6, SDL3_ttf
3.2.2 (with FreeType 2.14.3) and SDL3_mixer 3.2.4 at pinned versions,
checked by checksum (`FetchContent`), and builds them as static libraries
with only what the game uses: PNG through stb_image, MP3 through dr_mp3;
no GPU API, gamepads, camera, sensors or dialogs. It exposes them as
`wolfenstein::sdl3`, `wolfenstein::sdl3_image`, `wolfenstein::sdl3_ttf`
and `wolfenstein::sdl3_mixer`; modules link those names and never care
where a library came from. The libraries are optimised in debug builds
too, as a system's packages would be: the tests draw through SDL's
software renderer, several times slower unoptimised.

For multiplayer, native builds also fetch uWebSockets (the server's
WebSockets, with its event loop uSockets, built without TLS or
compression) and IXWebSocket (the native game's WebSocket client, with
TLS through the system's library where there is one: Apple's on macOS,
OpenSSL on Linux when it is installed); the browser has its own
WebSocket. nlohmann/json is downloaded at a pinned version with a
checksum; GoogleTest and Google Benchmark likewise, for native builds
only.

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
HUD font and the `TextureManager`. The window is made from properties,
centred; the renderer scales pictures with the nearest pixel (SDL 3
smooths them unless told otherwise), and draws at the configured size
through a **logical presentation**: SDL scales the picture to the window,
keeping its shape, letterboxed. In the browser the window takes the size
the page gives the canvas, so the picture scales with the page, and menu
clicks are turned into the picture's coordinates
(`SDL_ConvertEventToRenderCoordinates`). Besides creating them, its constructor
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
| `RendererContext` | No vsync (the game paces itself) | Vsync on: SDL 3 paces the page's loop by it |
| `net::Connection` | IXWebSocket, on a thread of its own | The browser's WebSocket (`emscripten/websocket.h`) |
| `Game::CheckGameEvent` | Esc pauses | Losing pointer lock pauses (the browser eats Esc) |
| `Game::CanvasStretch` | 1 | The canvas's CSS width over the window's |
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
- **SDL built from source, not from system packages or Emscripten's
  ports.** The same version everywhere, and only what the game uses (the
  web build's WebAssembly shrank by a quarter); the cost is a longer first
  build. Emscripten has no port of SDL3_image or SDL3_mixer, and Linux
  distributions lag behind (see [Moving to SDL 3](../features/sdl3.md)).

## Pitfalls

- **Headless OpenGL.** SDL 3 on Linux presents even its software renderer
  through OpenGL when it can; without a display that loads Mesa's
  llvmpipe, whose shader compiler crashed under AddressSanitizer on x86.
  Headless runs (CI, `scripts/dev.sh`) set
  `SDL_FRAMEBUFFER_ACCELERATION=0`, so no GL driver is loaded.
- **`exit()` on missing resources.** `RendererContext` and the texture
  lookups end the program on failure, which is right for a game but means
  those paths cannot be unit-tested; tests use placeholder textures.
- **Static library cycles.** Some modules link each other both ways (see
  [The big picture](../architecture/index.md#the-module-dependency-graph));
  CMake copes, but the layering is not enforced by the build.

## Possible improvements

- A `native-release` build with link-time optimisation.
- Gamepad input and a fullscreen toggle through SDL.
