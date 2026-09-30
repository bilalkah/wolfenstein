# Building with Emscripten

## Purpose

[Emscripten](https://emscripten.org) compiles C and C++ to WebAssembly with
Clang, and supplies what a native program expects from its platform: a C
library, a file system, and browser APIs behind C interfaces (WebSockets
among them); libraries such as SDL build for it from source as they do
natively. This page covers how the game's CMake build uses it.

## The toolchain

- **The SDK version is pinned: 6.0.10** everywhere. CI's web job runs in
  `emscripten/emsdk:6.0.10` (`.github/workflows/ci.yml`), the Pages
  workflow installs the same version with `emscripten-core/setup-emsdk`,
  and `docker/web.Dockerfile` and `scripts/build_web.sh` use the same
  image (the script prefers a local SDK if `emcmake` is on the `PATH`).
- The `web-release` CMake preset points CMake at the SDK's toolchain file
  (`$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`),
  builds `Release` and turns warnings into errors.

## Flags

SDL 3 and its libraries (image, TrueType text, mixing) are not
Emscripten ports: `cmake/Dependencies.cmake` downloads and builds them
with the game, as it does natively; SDL's own CMake build picks its
Emscripten backends (the canvas, WebGL, Web Audio, DOM events). The flags
the game adds:

| Flag | Where | What it does |
| --- | --- | --- |
| `--preload-file assets@/assets` | `app/CMakeLists.txt` | Packs every file under `assets/` into `index.data`, mounted at `/assets` |
| `--shell-file web/shell.html` | same | The page template the output `index.html` is made from |
| `-sALLOW_MEMORY_GROWTH=1` | same | Lets the WebAssembly heap grow past its initial size |
| `-lwebsocket.js` | `src/Net/CMakeLists.txt` | The browser's WebSocket behind `emscripten/websocket.h`, for multiplayer |

The output target is renamed so the build emits `index.html`, `index.js`,
`index.wasm` and `index.data`: a static site that can be served as is.

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

With Docker alone, the image builds the game and serves it
(see [Building](../building.md)):

```bash
docker build -f docker/web.Dockerfile -t wolfenstein-web .
docker run --rm -p 8000:8000 wolfenstein-web      # http://localhost:8000
```

While changing the code, the scripts build incrementally instead:

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
  textures. `build_web.sh` and `docker/web.Dockerfile` refuse to build in
  that case.
- **First build is slow.** SDL and its libraries compile with the game
  (a few minutes); later builds rebuild only what changed. The Docker
  scripts keep Emscripten's own cache (its C library and runtime) in a
  named volume.
- **Sanitizers** are native only (`WOLFENSTEIN_SANITIZERS` fails on the
  web build).
