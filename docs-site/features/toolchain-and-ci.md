# The toolchain and CI

| | |
| --- | --- |
| **When** | 21 to 25 September 2026 |
| **Commits** | `f84975d` Build with C++23, `a2929f3` Move to target-based CMake, Clang/libc++ everywhere, presets and tests, `37effdb` Apply clang-format to all sources, `a220068` Replace CI with native, sanitizer, web and format jobs, `3b1d817` Add a clang-tidy baseline, `8114156` Give every data member a default initialiser, `5d62b2a` build warning-free with `-Werror`, `bd577fd` enforce clang-tidy, `58f6c5b` Run CI on pushes to master only |
| **Code today** | [Platform layer](../engine/platform.md), [Profiling and testing](../engine/profiling.md#ci) |

## Problem

The 2024 build predated tests and had CI that "installed packages that no
longer exist on Ubuntu 24.04 (libtiff5-dev) and ran ctest with no tests
registered" (`a220068`). Before changing the engine deeply, the project
needed a build it could trust on every platform.

## Constraints

- One C++23 feature set on the web, macOS and Linux.
- Native and web builds from the same CMake files.
- Warnings, sanitizers, formatting and static analysis enforced, not
  advisory.

## Approach

- **C++23, extensions off** (`f84975d`): "so the code is checked against
  ISO C++".
- **Clang and libc++ everywhere** (`a2929f3`): "probing every target
  showed `std::mdspan` is missing even in GCC 15, so native builds now use
  Clang with libc++, the same standard library as the Emscripten and macOS
  builds: one C++23 feature set and identical container behaviour on web,
  desktop and a future Linux server."
- **Target-based CMake**: dependencies as `wolfenstein::` targets, one
  options target for every first-party target, presets for debug,
  release, sanitizers and the web; "the vendored 24,765-line json.hpp is
  replaced by a pinned, hash-checked download".
- **CI** (`a220068`): native debug and ASan builds in an Ubuntu 26.04
  container with the same toolchain as `scripts/dev.sh`, the web build in
  the pinned `emsdk 6.0.10` image, and a formatting check.
- **Static analysis** (`3b1d817`): `clang-tidy` with the bugprone,
  performance and modernize families and core-guidelines checks; a baseline
  of 322 findings, later driven to zero and made blocking (`bd577fd`).
- **Warnings as errors** (`5d62b2a`): "42 warnings had accumulated; they are
  fixed and every preset now builds with -Werror, so local builds and CI
  fail on the same warnings."

## C++ techniques used

- [Special members and pinned types](../techniques/special-members.md) and
  default member initialisers everywhere (`8114156`), the two largest
  families of tidy findings.
- The strict warning set: `-Wconversion`, `-Wsign-conversion`, `-Wshadow`,
  `-Wold-style-cast`, `-Wnon-virtual-dtor`, `-Woverloaded-virtual`,
  `-Wnull-dereference`, `-Wimplicit-fallthrough`.

## Key code

- [`cmake/ProjectOptions.cmake`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/cmake/ProjectOptions.cmake)
- [`cmake/Dependencies.cmake`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/cmake/Dependencies.cmake)
- [`CMakePresets.json`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/CMakePresets.json)
- [`.github/workflows/ci.yml`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/.github/workflows/ci.yml)

## Pitfalls

- AddressSanitizer found the state machines' `shared_ptr` cycle
  ("1,004 bytes in 12 allocations") as soon as the ASan job ran the
  benchmark; leak detection was switched off for that step until
  `39846c6` fixed it.
- In the dev container the repository is mounted at `/src`, so a tidy
  filter matching `src/` also matched downloaded headers; dependencies are
  excluded explicitly (`3b1d817`).

## What I'd change

- Cache the SDL ports and GoogleTest downloads in CI between runs.
- Build `docker/web.Dockerfile` in CI, so the documented way to build and
  play the game is checked on every change.
