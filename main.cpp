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

int
main()
{
  // Initialize Game
  wolfenstein::Pose2D pose(5, 1.5, 0.0);
  wolfenstein::PlayerConfig player_config(pose, 2.0, 1.0);
  wolfenstein::GeneralConfig config(1024, 768, 0, 50, 60.0, 60);

  wolfenstein::Game game(config, player_config);
  game.Run();

  return 0;
}