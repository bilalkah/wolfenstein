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
#include <SDL2/SDL.h>

/**
 * @brief Game class
 *
 */
class Game
{
public:
  Game();
  ~Game();
  void Run();

private:
  void Init();
  void Update();
  void Render();
  void Clean();

  bool isRunning;
  SDL_Window *window;
  SDL_Renderer *renderer;
};
