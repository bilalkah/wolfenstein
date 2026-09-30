# Stack and why

## Language and build

| Choice | Why |
| --- | --- |
| **C++23** | `std::expected` for errors as values, `std::mdspan`, ranges, `std::pmr` allocators, `std::format`: modern code with no runtime cost |
| **Clang and libc++ on every platform** | One compiler and one standard library for Linux, macOS and Emscripten; GCC's libstdc++ lacks `<mdspan>` |
| **CMake presets** | `native-debug`, `native-release`, `native-asan`, `web-release`: the same commands locally and in CI |
| **FetchContent, pinned by checksum** | Every library downloaded at a fixed version and built with the game: reproducible, nothing vendored |
| **Docker** | The dev toolchain, the web build and the server each come as a container: Docker is the only tool a machine needs |

## Platform

| Choice | Why |
| --- | --- |
| **SDL 3** (window, input, rendering, audio) | One portable layer for native and web |
| **SDL3_image, SDL3_ttf, SDL3_mixer** | PNG textures, the UI's fonts, MP3 music |
| **Built from source** | The same version everywhere, and only what the game uses (a quarter less WebAssembly). Emscripten has no SDL3_image or SDL3_mixer port, and Linux distributions lag behind |
| **SDL_Renderer (GPU: Metal, Direct3D, OpenGL, WebGL)** | Textured quads are all a raycaster needs: no shaders to write or port |
| **Emscripten → WebAssembly** | The same C++ runs in the browser |
| **Single-threaded web build** | Browser threads need special headers that GitHub Pages cannot send; single-threaded, any static host works |

## Libraries

| Choice | Why |
| --- | --- |
| **nlohmann/json** | The game's content is data: `config.json` as a tree, level files streamed through its SAX interface |
| **uWebSockets** | The multiplayer server's WebSockets: small, fast, an event loop with timers |
| **IXWebSocket** | The native game's WebSocket client, with TLS from the system |
| **The browser's WebSocket** (through Emscripten) | The web game's connection, no library needed |
| **GoogleTest, Google Benchmark** | Unit tests; micro-benchmarks |

## Networking

| Choice | Why |
| --- | --- |
| **An authoritative server** | One place decides every hit, so eight games agree and a cheat can only lie about its own input |
| **WebSocket, not WebRTC or UDP** | Works from any browser to any server; WebRTC would need signalling and relay (TURN) servers |
| **Caddy in front on the internet** | Gets and renews the certificate (Let's Encrypt) that `wss://` needs from an https page |

## Quality

| Choice | Why |
| --- | --- |
| **AddressSanitizer + UndefinedBehaviorSanitizer** | Memory and undefined-behaviour bugs caught in tests |
| **clang-tidy, clang-format** | One style and a set of bug-prone patterns checked on every change |
| **An allocation-counting allocator** | Enforces the zero-allocation rule: the benchmark and a scripted session fail on any allocation |
| **GitHub Actions** | Tests, sanitizers, static analysis, formatting and the browser benchmark on every push |
| **GitHub Pages + MkDocs Material** | This site and the playable game, published on every push to `master` |

## Content

| Choice | Why |
| --- | --- |
| **Freedoom** art, sounds and music (BSD) | Complete, consistent, free to ship |
| **Levels generated from layouts** (`scripts/make_levels.py`, `make_arenas.py`) | Levels are drawn as text, checked (reachability, doors, spawns) and written as data |
