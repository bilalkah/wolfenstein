# Profiling and testing

## Purpose

An engine that promises "no allocation after startup" and "the same
commands always play out the same way" needs machinery that checks those
promises on every change. This page covers the in-engine profiler, the
two scripted runs (the benchmark and the soak session), the unit tests,
and the CI jobs that run them all.

Code: `src/Profiler/`, `Game::StartBenchmark`, `Game::StartSoak`,
`Game::SoakStep`, `tests/`, `benchmarks/`, `scripts/alloc_breakdown.py`,
`scripts/check_soak.py`, `.github/workflows/ci.yml`.

## Concepts

### Instrumenting sections

A frame is split into named **sections** (update enemies, pathfinding,
line of sight, camera, walls, objects, draw, HUD, present). A
**scoped timer** notes the time when it is created and adds the elapsed
time to its section when it goes out of scope (RAII). Recording is
switched off in normal play, so a timer then costs two clock reads and a
branch.

### Scripted, reproducible runs

Measuring requires the same work each time: the same level, the same path,
the same enemies doing the same things, the same frame times. With a
deterministic simulation and a fixed time step, a scripted run is exactly
reproducible, so two builds can be compared frame by frame.

## How it is implemented here

### Sections that count allocations too

```cpp title="src/Profiler/src/profiler.cpp"
ScopedTimer::ScopedTimer(ProfileSection section)
    : section_(section),
      start_(std::chrono::steady_clock::now()),
      start_allocations_(AllocationStats::count),
      start_bytes_(AllocationStats::bytes) {}

ScopedTimer::~ScopedTimer() {
    auto& profiler = Profiler::GetInstance();
    if (profiler.IsEnabled()) {
        const std::chrono::duration<double, std::milli> elapsed =
            std::chrono::steady_clock::now() - start_;
        profiler.Add(section_, elapsed.count(),
                     AllocationStats::count - start_allocations_,
                     AllocationStats::bytes - start_bytes_);
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Profiler/src/profiler.cpp#L153-L168){ .excerpt-source }

Each section records time **and** the heap allocations made while it ran
(read from the counter described in [Memory](memory.md)), so a report
points at the code that allocates, not just the frame.
`scripts/alloc_breakdown.py` prints the report as a tree of sections.

### The benchmark

`wolfenstein --benchmark N` (or `index.html?benchmark=N` in a browser)
skips the menu and plays N frames of a fixed scenario at a fixed 1/60 s
step: the player walks a set route through level 1 facing forward and back
while its enemies chase and attack, with the walls along the route already
marked by shots (as many marks as a level keeps, so decals are drawn too).
No input device is read. At the end it prints a `BENCHMARK_RESULT` line
of JSON: startup and level-load time and allocations, then per-section
statistics and the raw per-frame samples.

- `scripts/bench_web.sh` builds the web version and runs the benchmark in
  headless Chromium (`benchmarks/run_web_benchmark.mjs`, with Playwright),
  keeping each report in `docs/benchmarks/` (git-ignored, local records)
  and printing the change against the previous run.
- `benchmarks/micro/` holds Google Benchmark micro-benchmarks, for example
  of the arena against `new`/`delete` and `std::pmr::monotonic_buffer_resource`.

### The soak session

`wolfenstein --soak` (or `?soak`) plays a scripted session through
everything a player can reach, starting from the main menu: the 3D view,
the developer's 2D view, the map, pause, the settings screen, a pickup, a
door, a level transition (results, story, briefing), dying, a new game,
and playing again. It counts allocations per phase and prints a
`SOAK_RESULT` line; `scripts/check_soak.py` fails unless nothing was
allocated after the first game started **and** the session really went
where the script sends it (it reached the second level, saw the results,
a briefing and a page of story, took a pickup, opened a door, found a
secret).

### Unit tests

The `tests/` directory holds GoogleTest suites (331 `TEST` definitions in
45 files; with the level-design test instantiated for each of the fifteen
campaign levels, 345 tests run). They exercise the simulation without a
window or an audio device: a `World` or a `Scene` built directly, with
placeholder textures (`test_services.h`) and small maps written on the fly
(`test_map.h`). A few highlights:

| Test file | Checks |
| --- | --- |
| `level_design_test.cpp` | Every shipped level is playable (see [Levels](levels.md)) |
| `tactics_test.cpp` | Enemies fight from their range, sidestep, spread out, retreat |
| `simulation_test.cpp` | Commands drive the simulation: the same commands give the same game |
| `fixed_step_test.cpp` | Frame time turned into fixed ticks, stalls dropped |
| `world_test.cpp` | New games, level transitions, saving and continuing, zero allocations while playing |
| `spatial_sound_test.cpp` | Panning and falloff; the mixer's voices |
| `monotonic_arena_test.cpp`, `object_pool_test.cpp` | The allocators, including stale handles |
| `view_test.cpp` | The sky's layout, view angles, the kick |
| `intel_test.cpp` | Reading pages of intel on walls |

Tests built with `WOLFENSTEIN_COUNTS_ALLOCATIONS` also assert that
specific operations allocate nothing.

### CI

`.github/workflows/ci.yml` runs on pushes to `master` and on pull requests:

| Job | What it runs |
| --- | --- |
| Native (debug, ASan+UBSan) | Build, all unit tests, the benchmark; on the debug build, the zero-allocation gate on the benchmark and the soak |
| Web (Emscripten 6.0.10) | The WebAssembly build, kept as an artifact |
| Web benchmark (headless Chromium) | The benchmark and the soak in a browser, both required to allocate nothing |
| Formatting | `clang-format --dry-run -Werror` on every source |
| Static analysis | `clang-tidy` over first-party code, failing on any finding |

Native builds use Clang with libc++ (the same standard library as
Emscripten and macOS), `-Werror`, and a strict warning set
(`-Wconversion`, `-Wsign-conversion`, `-Wshadow`, `-Wold-style-cast`, ...).

## Design decisions and trade-offs

- **Allocation counts beside timings.** Time varies with the machine;
  allocation counts do not, so they make a stable CI gate.
- **Scripted runs in the real executable.** The benchmark and soak run the
  real game loop and renderer (headless natively, with SDL's software
  renderer and dummy audio), so they catch what unit tests cannot.
- **Tests without a GPU.** The simulation's independence from the
  presentation is what makes most of the game unit-testable.

## Pitfalls

- **Headless native rendering allocates inside SDL** (the software
  renderer), which is why the native counter counts only `operator new`;
  the web counter, in a real browser with WebGL, counts `malloc` itself.
- **Timings in CI are noisy.** The web benchmark's timing figures from a
  shared runner are for trends, not gates; the allocation figures gate.

## Possible improvements

- Record a player's commands and replay them as a test; with determinism,
  any divergence is a bug.
- Track the benchmark's timings over time in a dashboard (the reports are
  already JSON).
