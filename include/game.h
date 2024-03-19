/**
 * @file game.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Game class definition
 * @version 0.1
 * @date 2024-02-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef GAME_H_
#define GAME_H_

#include "clock.h"
#include "config.h"
#include "map.h"
#include "player.h"
#include <SDL2/SDL.h>

/**
 * @brief Game class
 *
 */
namespace wolfenstein
{

enum class RenderType
{
  RENDER2D,
  RENDER3D,
};

class Game
{
public:
  Game(GeneralConfig config, PlayerConfig player_config);
  ~Game();
  void Run();

private:
  void Init();
  void CheckEvent();
  void Render();

  void RenderMainScreen();
  void RenderTextFPS();
  void RenderFooter();

  void Render2D();
  void RenderMap2D();
  void RenderPlayer2D();

  void Render3D();

  void Clean();

  // Clock
  Clock clock_;

  // Config
  GeneralConfig config_;

  // Map
  Map map_;

  // Moving Objects
  Player player_;

  // SDL
  SDL_Window *window_;
  SDL_Renderer *renderer_;
  bool isRunning_;
  RenderType render_type_;
};

} // namespace wolfenstein

#endif
