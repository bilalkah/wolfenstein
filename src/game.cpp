#include "include/game.h"
#include "include/macros.h"

Game::Game()
{
  isRunning = false;
  window = nullptr;
  renderer = nullptr;
}

Game::~Game() { Clean(); }

void
Game::Init()
{
  if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
      DEBUG_MSG("SDL could not initialize! SDL_Error: " << SDL_GetError());
      isRunning = false;
    }
  else
    {
      window = SDL_CreateWindow("Some Game", SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, 640, 480,
                                SDL_WINDOW_SHOWN);
      if(window == nullptr)
        {
          DEBUG_MSG(
              "Window could not be created! SDL_Error: " << SDL_GetError());
          isRunning = false;
        }
      else
        {
          renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
          if(renderer == nullptr)
            {
              DEBUG_MSG("Renderer could not be created! SDL_Error: "
                        << SDL_GetError());
              isRunning = false;
            }
          else
            {
              isRunning = true;
            }
        }
    }
}

void
Game::Update()
{
  // Check for events
  SDL_Event event;
  while(SDL_PollEvent(&event))
    {
      if(event.type == SDL_QUIT)
        {
          isRunning = false;
        }
    }
}

void
Game::Render()
{
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
}

void
Game::Clean()
{
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void
Game::Run()
{
  Init();
  while(isRunning)
    {
      Update();
      Render();
    }
  DEBUG_MSG("Game is closing...");
}
