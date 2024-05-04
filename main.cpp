/**
 * @file main.cpp
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "include/config.h"
#include "include/game.h"
#include "include/types.h"

int main() {

  wolfenstein::Pose2D pose(3, 1.5, 1.50);
  wolfenstein::PlayerConfig player_config(pose, 2.0, 1.3);
  wolfenstein::GeneralConfig config(1024, 768, 0, 50, 60.0, 60);

  wolfenstein::Game game(config, player_config);
  game.Run();

  return 0;
}