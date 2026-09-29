# Building

Everything builds in Docker, so Docker is the one tool a machine needs.
The WebAssembly build is the one to play, in any desktop browser; the
native build, in a Linux container, runs the tests, the benchmark and the
scripted sessions.

## Get the code and the assets

The assets (images, fonts, sounds, music) are stored with
[Git LFS](https://git-lfs.com); install it before cloning, or run
`git lfs pull` after.

```bash
git lfs install
git clone https://github.com/bilalkah/wolfenstein
cd wolfenstein
```

## Build and play in the browser

```bash
docker build -f docker/web.Dockerfile -t wolfenstein-web .
docker run --rm -p 8000:8000 wolfenstein-web
```

Then open <http://localhost:8000>; Ctrl+C stops the server. (The address
it prints for other machines is the container's own; from another machine,
use this one's address.)

`docker/web.Dockerfile` builds in two stages:

1. **Build**, in `emscripten/emsdk:6.0.10`, the Emscripten SDK that CI
   pins: `cmake --preset web-release`, then `cmake --build --preset
   web-release`. Emscripten downloads and compiles SDL2 and its satellite
   libraries as ports along with the game, which takes about two minutes.
   It stops early if the assets are Git LFS pointers rather than files.
2. **Serve**, in a small Python image: the four files of the build
   (`index.html`, `index.js`, `index.wasm`, `index.data`) and
   `scripts/serve_web.py`, which answers byte-range requests as the page's
   loader expects.

Only the sources the build needs go into the image
(`docker/web.Dockerfile.dockerignore`), not the build trees or the history.

### While changing the code

A change to any source rebuilds the image from that step, ports and all.
For quicker rounds, the scripts build incrementally into
`build/web-release`, with Emscripten still in Docker (a named volume keeps
SDL's compiled ports between builds), and serve the result with Python on
port 8000 and to the local network:

```bash
./scripts/build_web.sh   # build/web-release/bin
./scripts/run_web.sh     # builds, then serves on http://localhost:8000
```

With an Emscripten SDK installed locally (`emcmake` on the `PATH`),
`build_web.sh` uses it instead of the container. The output is a static
site; browsers refuse to load WebAssembly from `file://`, so it has to be
served over HTTP. See [Building with Emscripten](web/emscripten-build.md).

## The native build, tests and checks

The native build runs in a container too: `docker/dev.Dockerfile`
(Ubuntu 26.04, LLVM 21, SDL2), which `scripts/dev.sh` builds on first use
and runs any command in, with the repository mounted. CI uses the same
toolchain.

```bash
./scripts/dev.sh cmake --preset native-debug
./scripts/dev.sh cmake --build --preset native-debug
./scripts/dev.sh ctest --preset native-debug
```

The toolchain is Clang with libc++ (the standard library the Emscripten
build uses too; libstdc++ lacks `<mdspan>`), CMake 3.25 or newer, and SDL2
with SDL_image, SDL_ttf and SDL_mixer. The container has no display or
sound card: SDL runs off-screen, so there the game is built, tested and
measured, and the browser is where it is played.

### Presets

| Preset | What |
| --- | --- |
| `native-debug` | Debug build, tests, the allocation gates |
| `native-release` | Optimised with debug info, for benchmarking |
| `native-asan` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `web-release` | The WebAssembly build (`docker/web.Dockerfile`, `scripts/build_web.sh`) |

All presets build with `-Werror`.

### Scripted runs

```bash
./scripts/dev.sh bash -c "./build/native-release/bin/wolfenstein --benchmark 2000 | python3 scripts/alloc_breakdown.py"
./scripts/dev.sh bash -c "./build/native-debug/bin/wolfenstein --soak | python3 scripts/check_soak.py"
```

In the browser the same runs are `?benchmark=2000` and `?soak` in the
address, and `?debug` lets **P** show the top-down view.

### Checks

```bash
./scripts/dev.sh ctest --preset native-debug        # unit tests
./scripts/dev.sh ./scripts/tidy.sh                   # clang-tidy, after configuring native-debug
```

## This documentation site

The site is built with [MkDocs](https://www.mkdocs.org) and
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/); its
sources are in `docs-site/`, its configuration in `mkdocs.yml`.

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements-docs.txt    # MkDocs, Material, glightbox, pinned
.venv/bin/mkdocs serve                 # http://127.0.0.1:8000/wolfenstein/
.venv/bin/mkdocs build --strict        # the site in site/, failing on any warning
```

### With the game

The game's page, `play/`, is not in the repository: CI copies the web
build there before building the site, and the home page links to it. To
preview the two together:

```bash
./scripts/build_web.sh
mkdir -p docs-site/play && cp build/web-release/bin/* docs-site/play/
.venv/bin/mkdocs serve
```

`docs-site/play/` and `site/` are git-ignored.

## Publishing

`.github/workflows/pages.yml` builds the WebAssembly version with the
pinned Emscripten SDK, copies it into `docs-site/play/`, builds the site
with `mkdocs build --strict`, and deploys it to GitHub Pages on every push
to `master` (or by hand, from the Actions tab). The repository's Pages
source must be set to **GitHub Actions**.
