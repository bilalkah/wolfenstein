# Building and hosting

The assets are stored with [Git LFS](https://git-lfs.com):

```bash
git lfs install
git clone https://github.com/bilalkah/wolfenstein
```

## Play in the browser

```bash
docker build -f docker/web.Dockerfile -t wolfenstein-web .
docker run --rm -p 8000:8000 wolfenstein-web      # http://localhost:8000
```

While changing the code, build incrementally and serve to the network:

```bash
./scripts/run_web.sh          # builds build/web-release, serves on port 8000
```

## Native

The toolchain (Clang, libc++, CMake) comes as a container; SDL 3 and every
other library are downloaded and built with the game.

```bash
./scripts/dev.sh cmake --preset native-debug
./scripts/dev.sh cmake --build --preset native-debug
./scripts/dev.sh ctest --preset native-debug
```

On macOS it builds and plays directly:

```bash
cmake --preset native-release && cmake --build --preset native-release
./build/native-release/bin/wolfenstein
```

| Preset | For |
| --- | --- |
| `native-debug` | Development, tests, the allocation checks |
| `native-release` | Playing, benchmarking |
| `native-asan` | Tests under AddressSanitizer and UndefinedBehaviorSanitizer |
| `web-release` | The WebAssembly build |

## Checks

```bash
./scripts/dev.sh ctest --preset native-debug                  # unit tests
./scripts/dev.sh ./scripts/tidy.sh                            # clang-tidy
./scripts/dev.sh bash -c "./build/native-release/bin/wolfenstein --benchmark 2000 | python3 scripts/alloc_breakdown.py"
./scripts/dev.sh bash -c "./build/native-debug/bin/wolfenstein --soak | python3 scripts/check_soak.py"
```

In the browser the same runs are `?benchmark=2000` and `?soak`; `?debug`
lets **P** show the top-down view with the rays and the enemies' paths.

## A multiplayer server

### On a local network

```bash
cmake --build --preset native-release --target wolfenstein-server
./build/native-release/bin/wolfenstein-server          # port 8080
./scripts/run_web.sh                                   # the web game, port 8000
```

Players open `http://<host>:8000`, choose **MULTIPLAYER** and join: the
page offers the server beside it. The native game joins with
`--connect ws://<host>:8080 --name ann`.

| Option | Default | |
| --- | --- | --- |
| `--port` | 8080 | |
| `--level` | every arena | `bazaar.json,warehouse.json`, played in turn |
| `--mode` | `deathmatch` | or `gunrace` |
| `--frags` | 20 | frag limit |
| `--minutes` | 10 | time limit |

### In Docker

```bash
docker build -f docker/server.Dockerfile -t wolfenstein-server .
docker run --rm -p 8080:8080 wolfenstein-server
```

### On the internet

An https page may only open `wss://`, so the server needs a certificate.
`docker/compose.yml` runs the server, the web game, and Caddy in front
(certificate from Let's Encrypt):

```bash
DOMAIN=play.example.com docker compose -f docker/compose.yml up -d --build
```

The domain must point at the machine, with ports 80 and 443 open.

## This site

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements-docs.txt
.venv/bin/mkdocs serve             # http://127.0.0.1:8000/wolfenstein/
```

CI builds the game and this site and publishes both to GitHub Pages on
every push to `master`.
