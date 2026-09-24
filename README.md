# Wolfenstein Project

Welcome to the Wolfenstein project! This README provides an overview of the game's features, setup instructions, and how to contribute to the development. Dive into a dynamic and immersive world built using **C++** and **SDL**, where you'll experience real-time combat and a rich environment.

## View from the game

<p float="left">
  <img src="images/game_view_with_mp5.png" width="400" />
  <img src="images/game_view_with_shotgun.png" width="400" /> 
</p>

---

## Features

### Core Gameplay
- **Dynamic Combat**: Engage in fast-paced action using a variety of weapons.
- **Pathfinding with A***: Enemies dynamically navigate the environment using the A* algorithm.
- **Player Interactions**: Smooth controls for movement and combat.

### Environment
- **Static Maps with Dynamic Elements**: Navigate a static map populated with dynamic objects and enemies.

### Enemy Behavior
- **State Design Pattern**: Enemies utilize a flexible state system with the following behaviors:
  - Idle
  - Walk
  - Attack
  - Pain
  - Death

### Weapons
- **State Design Pattern**: Weapons use states for the following functionalities:
  - Loaded
  - OutOfAmmo
  - Reloading
- **Synchronized Execution**: All weapon and enemy animations are synchronized with the main game clock to maintain fluid gameplay.

---

## Technical Highlights

- **Pathfinding and Navigation**: A* algorithm is used for efficient enemy movement and navigation. For more details on the implementation, see my [path-planning](https://github.com/bilalkah/path-planning) project.
- **Ray Casting with DDA**: The Digital Differential Analyzer (DDA) algorithm is used for efficient ray casting, creating realistic field of view.
- **Templates and Type Traits**: Advanced template programming ensures modular and reusable code.
- **Instant State Transitions**: Transitions between states happen instantly, ensuring smooth gameplay.

---

## Setup Instructions

Clone the repository with its submodule and the assets, which are stored with [Git LFS](https://git-lfs.com):
```bash
git lfs install
git clone https://github.com/bilalkah/wolfenstein --recurse-submodules
cd wolfenstein
```

### Play in the Browser (WebAssembly)

The game compiles to WebAssembly with [Emscripten](https://emscripten.org) and runs in any desktop browser.

1. Build it. This uses your local Emscripten SDK if `emcmake` is on your `PATH`, otherwise the `emscripten/emsdk` Docker image:
   ```bash
   ./scripts/build_web.sh
   ```
2. Serve it and open http://localhost:8000:
   ```bash
   ./scripts/run_web.sh
   ```

The output in `build/web-release/bin` is a static site (`index.html`, `.js`, `.wasm`, `.data`), so it can also be hosted as is, for example on GitHub Pages.

### Benchmark

`index.html?benchmark=2000` runs a fixed, reproducible scenario instead of the game: the player walks a set route through level 1 at a fixed time step while enemies chase and attack. It reports per-frame timings for each part of the frame (AI, pathfinding, raycasting, rendering) and heap allocations per frame.

```bash
./scripts/bench_web.sh [frames] [label]
```

This builds the web version, runs the benchmark in headless Chromium (Docker), saves the full report to `docs/benchmarks/results/`, appends a row to `docs/benchmarks/results.md` (both git-ignored, local records) and prints the change against the previous run. Natively, run `./build/bin/wolfenstein --benchmark 2000`.

### Native Build

The native build uses **Clang with libc++** (the same standard library as the web and macOS builds) and CMake presets.

The easiest way is the toolchain container, which only needs Docker:
```bash
./scripts/dev.sh cmake --preset native-debug          # configure
./scripts/dev.sh cmake --build --preset native-debug  # build
./scripts/dev.sh ctest --preset native-debug          # unit tests
```

On Ubuntu 26.04 you can also install the toolchain directly with `./scripts/install_deps.sh` and drop the `./scripts/dev.sh` prefix.

| Preset | Purpose |
|---|---|
| `native-debug` | Development build with tests |
| `native-release` | Optimised build with debug info (`./scripts/compile.sh`) |
| `native-asan` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `web-release` | WebAssembly build (`./scripts/build_web.sh`) |

The game binary is `build/<preset>/bin/wolfenstein`; `--benchmark 300` runs the headless benchmark scenario. Static analysis: `./scripts/dev.sh ./scripts/tidy.sh`.

---

## How to Play

- **Movement**: Use `W`, `A`, `S`, `D` to move around.
- **Turn**: Move the mouse, or use `Left Arrow` and `Right Arrow`.
- **Attack**: Use `Left Click` or `Left Ctrl` to attack enemies.
- **Reload**: Press `R`.
- **Select Weapon**: Use `Left Arrow`, `Right Arrow`, and `Space` to navigate and select your weapon.
- **Map View**: Press `P` to toggle the top-down view.
- **Browser**: Click the game to capture the mouse; `Esc` releases it.

---

## Contribution Guidelines

We welcome contributions! Here are a few ways you can help:

1. **Report Bugs**: Submit issues on the GitHub repository.
2. **Suggest Features**: Share your ideas to improve gameplay.
3. **Write Code**: Fork the repository and submit a pull request with your improvements.

### Development Workflow
1. Create a new branch for your feature:
   ```bash
   git checkout -b feature-name
   ```
2. Commit your changes:
   ```bash
   git commit -m "Description of changes"
   ```
3. Push the branch and open a pull request:
   ```bash
   git push origin feature-name
   ```

---

## Troubleshooting

### Common Issues
1. **Performance drops**:
   - Use caching mechanism to prevent extensively running pathfinding algorithm.
   - Optimize pathfinding algorithms to enemies do not block each other.
2. **Better to have**:
   - Collision check with static and dynamic objects
---

## Future Plans

- Expand maps and introduce new enemy types.

---

## Acknowledgments

Thank you for checking out the first version of my Wolfenstein game! This release represents my learning journey, including experimenting with game design, state management, and spatial data handling. I hope you enjoy playing it as much as I enjoyed creating it.

Additionally, I would also like to share the source of motivation and inspiration for me to do this project. 95% of the assets were obtained from the relevant source.

https://github.com/StanislavPetrovV/DOOM-style-Game  
https://www.youtube.com/watch?v=ECqUrT7IdqQ

---

## License

This project is open-source and available under the MIT License. See the `LICENSE` file for details.

---

## Contact

For questions or feedback, reach out at:
- Email: kahramannbilal@gmail.com
- GitHub: [bilalkah](https://github.com/bilalkah)

Enjoy the game and happy coding!

