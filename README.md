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

### Prerequisites
1. **C++ Compiler**: Ensure you have a modern C++ compiler (C++17 or higher).
2. **SDL Library**: Install SDL for rendering, sound, and input handling.

### Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/bilalkah/wolfenstein --recurse-submodules
   cd wolfenstein
   ```
2. Install dependencies:
   ```bash
   ./scripts/install_deps.sh
   ```
3. Build the project using `cmake` or your preferred build system:
   ```bash
   ./scripts/compile.sh # or you can your own way to build
   ```
4. Run the executable:
   ```bash
   ./build/bin/wolfenstein
   ```

---

## How to Play

- **Movement**: Use `W`, `A`, `S`, `D` to move around.
- **Attack**: Use `Left Click` to attack enemies.
- **Select Weapon**: Use `Left Arrow`, `Right Arrow`, and `Space` to navigate and select your weapon.

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

