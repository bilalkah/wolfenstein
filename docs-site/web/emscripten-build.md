# Building with Emscripten

## Purpose

[Emscripten](https://emscripten.org) compiles C and C++ to WebAssembly with
Clang, and supplies what a native program expects from its platform: a C
library, a file system, and ports of popular libraries (SDL2 among them)
implemented on browser APIs. This page covers how the game's CMake build
uses it.

## The toolchain

- **The SDK version is pinned in CI**: `emscripten/emsdk:6.0.10` (the web
  job in `.github/workflows/ci.yml`), and the Pages workflow installs the
  same version with `mymindstorm/setup-emsdk`. Local scripts use a local
  SDK if `emcmake` is on the `PATH`, otherwise the `emscripten/emsdk`
  Docker image.
- The `web-release` CMake preset points CMake at the SDK's toolchain file
  (`$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`),
  builds `Release` and turns warnings into errors.

## Flags

| Flag | Where | What it does |
| --- | --- | --- |
| `-sUSE_SDL=2` | `Dependencies.cmake` | SDL2, implemented on the canvas, WebGL, Web Audio and DOM events |
| `-sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS=png,jpg` | same | Image decoding, only the two formats used |
| `-sUSE_SDL_TTF=2` | same | TrueType fonts (FreeType) |
| `-sUSE_SDL_MIXER=2 -sSDL2_MIXER_FORMATS=mp3` | same | Mixing and music, with MP3 decoding for the music |
| `--preload-file assets@/assets` | `CMakeLists.txt` | Packs every file under `assets/` into `index.data`, mounted at `/assets` |
| `--shell-file web/shell.html` | same | The page template the output `index.html` is made from |
| `-sALLOW_MEMORY_GROWTH=1` | same | Lets the WebAssembly heap grow past its initial size |

The ports are flags on both compiling and linking: CMake interface
targets carry them, so a module linking `wolfenstein::sdl2_mixer` gets the
right flags on either platform. The output target is renamed so the build
emits `index.html`, `index.js`, `index.wasm` and `index.data`: a static
site that can be served as is.

### No threads

`cmake/ProjectOptions.cmake` links threads for native builds only:
"Browsers only allow threads on cross-origin isolated pages, so the web
build is single threaded." Threads in WebAssembly need
`SharedArrayBuffer`, which browsers enable only on pages served with the
`Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy` headers;
GitHub Pages cannot set headers, so a single-threaded build is also what
makes Pages hosting possible. The game has no threads of its own; the
audio mixer runs on the browser's audio callback.

### The shell page

`--shell-file` substitutes Emscripten's generated loader for
`{{{ SCRIPT }}}` in `web/shell.html`. Everything before it (the loader's
markup and styles, the `Module` object with its hooks, the download and
caching code, pointer lock and audio unlock) is the project's own. Assets
and the shell are listed as link dependencies, so editing either relinks.

## Building and serving

```bash
./scripts/build_web.sh   # build/web-release/bin: index.html, .js, .wasm, .data
./scripts/run_web.sh     # builds, then serves it on http://localhost:8000
```

`scripts/serve_web.py` is `python3 -m http.server` plus byte ranges (the
page downloads in resumable pieces) and resetting connections that stop
taking data. Browsers refuse to load WebAssembly from `file://` URLs, so a
server is needed even locally.

## Pitfalls

- **Git LFS.** The assets are LFS objects; a clone without LFS has pointer
  files, and packing those would produce a game that cannot load its
  textures. `build_web.sh` refuses to build in that case.
- **First build is slow.** Emscripten compiles the SDL ports on first use
  and caches them (the Docker scripts keep the cache in a named volume).
- **Sanitizers** are native only (`WOLFENSTEIN_SANITIZERS` fails on the
  web build).
