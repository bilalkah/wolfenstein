# Building

## Get the code and the assets

The assets (images, fonts, sounds, music) are stored with
[Git LFS](https://git-lfs.com); install it before cloning, or run
`git lfs pull` after.

```bash
git lfs install
git clone https://github.com/bilalkah/wolfenstein
cd wolfenstein
```

## Native builds

The native toolchain is Clang with libc++ (the standard library the
Emscripten and macOS builds use too; libstdc++ lacks `<mdspan>`), CMake
3.25 or newer, and SDL2 with SDL_image, SDL_ttf and SDL_mixer.

=== "In the dev container (any OS with Docker)"

    ```bash
    ./scripts/dev.sh cmake --preset native-debug
    ./scripts/dev.sh cmake --build --preset native-debug
    ./scripts/dev.sh ctest --preset native-debug
    ```

    `scripts/dev.sh` builds `docker/dev.Dockerfile` (Ubuntu 26.04, LLVM 21,
    SDL2) on first use and runs the command inside it, with the repository
    mounted.

=== "On Ubuntu 26.04"

    ```bash
    ./scripts/install_deps.sh        # Clang, libc++, clang-format, clang-tidy, SDL2
    cmake --preset native-release
    cmake --build --preset native-release
    ./build/native-release/bin/wolfenstein
    ```

=== "Elsewhere"

    Install Clang, libc++ and SDL2 with its three satellite libraries
    (for example with Homebrew on macOS), then use the presets as above.

    !!! todo "Bilal: explain the macOS setup you use"
        The scripts target Ubuntu and Docker; the repository does not
        record how the native macOS build is set up. A guess: the game is
        built and tested in the dev container and played on macOS through
        the web build; a native macOS build would use Apple's Clang (whose
        standard library is libc++) with SDL2 from Homebrew.

### Presets

| Preset | What |
| --- | --- |
| `native-debug` | Debug build, tests, the allocation gates |
| `native-release` | Optimised with debug info, for playing and benchmarking |
| `native-asan` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `web-release` | The WebAssembly build (needs `$EMSDK`) |

All presets build with `-Werror`.

### Running

```bash
./build/native-release/bin/wolfenstein              # the game
./build/native-release/bin/wolfenstein --debug      # P shows the 2D view
./build/native-release/bin/wolfenstein --benchmark 2000
./build/native-release/bin/wolfenstein --soak
```

### Checks

```bash
./scripts/dev.sh ctest --preset native-debug        # unit tests
./scripts/dev.sh bash -c "./build/native-debug/bin/wolfenstein --soak | python3 scripts/check_soak.py"
./scripts/dev.sh ./scripts/tidy.sh                   # clang-tidy, after configuring native-debug
```

## The WebAssembly build

=== "With the build script"

    ```bash
    ./scripts/build_web.sh   # local SDK if emcmake is on PATH, else the emsdk Docker image
    ./scripts/run_web.sh     # builds, then serves on http://localhost:8000
    ```

=== "With a local Emscripten SDK"

    ```bash
    git clone https://github.com/emscripten-core/emsdk.git
    ./emsdk/emsdk install 6.0.10     # the version CI pins
    ./emsdk/emsdk activate 6.0.10
    source ./emsdk/emsdk_env.sh
    cmake --preset web-release
    cmake --build --preset web-release
    python3 scripts/serve_web.py build/web-release/bin 8000
    ```

The output in `build/web-release/bin` (`index.html`, `index.js`,
`index.wasm`, `index.data`) is a static site. Browsers refuse to load
WebAssembly from `file://`, so serve it over HTTP. See
[Building with Emscripten](web/emscripten-build.md).

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
