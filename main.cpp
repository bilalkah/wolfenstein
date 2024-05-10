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
#include "include/utils.h"

int main() {

  wolfenstein::Position2D position(3, 1.5, 1.50);
  wolfenstein::PlayerConfig player_config(position, 2.0, 0.5);
  wolfenstein::GeneralConfig config(1920, 1080, 0, 50, 120, 15.0,
                                    wolfenstein::utility::ToRadians(60.0));

  wolfenstein::Game game(config, player_config);
  game.Run();

  return 0;
}