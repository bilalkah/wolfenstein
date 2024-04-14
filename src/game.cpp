#include "include/game.h"
#include "include/macros.h"
#include <SDL2/SDL_ttf.h>

namespace utils
{
void
drawCircle(SDL_Renderer *renderer, int centerX, int centerY, int radius)
{
  // Draw circle using points along the circumference
  const int num_points = 100;
  const double increment = 2 * M_PI / num_points;

  for(int i = 0; i < num_points; ++i)
    {
      double angle = i * increment;
      int x = static_cast<int>(centerX + radius * std::cos(angle));
      int y = static_cast<int>(centerY + radius * std::sin(angle));

      SDL_RenderDrawPoint(renderer, x, y);
    }
}
} // namespace utils

namespace wolfenstein
{

Game::Game(GeneralConfig config, PlayerConfig player_config)
    : config_(config), player_(Player(player_config))
{
  isRunning_ = false;
  window_ = nullptr;
  renderer_ = nullptr;
}

Game::~Game() { Clean(); }

void
Game::Init()
{
  if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
      DEBUG_MSG("SDL could not initialize! SDL_Error: " << SDL_GetError());
      isRunning_ = false;
    }
  else
    {
      window_ = SDL_CreateWindow("Some Game", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, config_.width_,
                                 config_.height_, SDL_WINDOW_SHOWN);
      if(window_ == nullptr)
        {
          DEBUG_MSG(
              "Window could not be created! SDL_Error: " << SDL_GetError());
          isRunning_ = false;
        }
      else
        {
          renderer_
              = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
          if(renderer_ == nullptr)
            {
              DEBUG_MSG("Renderer could not be created! SDL_Error: "
                        << SDL_GetError());
              isRunning_ = false;
            }
          else
            {
              isRunning_ = true;
            }
        }
    }
  if(TTF_Init() < 0)
    {
      DEBUG_MSG("TTF could not initialize! TTF_Error: " << TTF_GetError());
      isRunning_ = false;
    }
  map_.LoadMap();
  player_.SetMap(std::make_shared<Map>(map_));
  clock_.InitClock();
  render_type_ = RenderType::RENDER2D;
}

void
Game::CheckEvent()
{
  // Check for events
  SDL_Event event;
  while(SDL_PollEvent(&event))
    {
      if(event.type == SDL_QUIT
         || (event.type == SDL_EventType::SDL_KEYDOWN
             && event.key.keysym.sym == SDLK_ESCAPE))
        {
          isRunning_ = false;
        }
      if(event.type == SDL_EventType::SDL_KEYDOWN
         && event.key.keysym.sym == SDLK_ASTERISK)
        {
          if(render_type_ == RenderType::RENDER2D)
            {
              render_type_ = RenderType::RENDER3D;
            }
          else
            {
              render_type_ = RenderType::RENDER2D;
            }
        }
    }
}

void
Game::Render()
{
  // Clear
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  SDL_RenderClear(renderer_);

  // Update
  RenderMainScreen();

  // Update Screen
  SDL_RenderPresent(renderer_);
}

void
Game::RenderMainScreen()
{
  switch(render_type_)
    {
    case RenderType::RENDER2D:
      Render2D();
      break;

    case RenderType::RENDER3D:
      Render3D();
      break;

    default:
      DEBUG_MSG("Something cooked here");
      break;
    }

  RenderTextFPS();
}

void
Game::Render2D()
{
  RenderMap2D();
  RenderPlayer2D();
}

void
Game::RenderMap2D()
{
  auto map_data = map_.GetMap();
  auto row_size = map_data.size();
  auto col_size = map_data[0].size();

  for(ui16 i = 0; i < col_size; i++)
    {
      for(ui16 j = 0; j < row_size; j++)
        {
          if(map_data[j][i] != 0)
            {
              SDL_Rect rect = { config_.padding_ + j * config_.scale_,
                                config_.padding_ + i * config_.scale_,
                                config_.scale_, config_.scale_ };
              SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
              SDL_RenderDrawRect(renderer_, &rect);
            }
        }
    }
}

void
Game::RenderPlayer2D()
{
  Pose2D pose{ player_.GetPose() };
  ui16 radius{ 10 };
  SDL_SetRenderDrawColor(renderer_, 0xFF, 0x00, 0x00, 0xFF); // red color
  utils::drawCircle(renderer_, pose.x_ * config_.scale_,
                    pose.y_ * config_.scale_, radius);
  SDL_RenderDrawLine(
      renderer_, pose.x_ * config_.scale_, pose.y_ * config_.scale_,
      pose.x_ * config_.scale_
          + map_.GetSizeX() * std::cos(pose.theta_) * config_.scale_,
      pose.y_ * config_.scale_
          + map_.GetSizeY() * std::sin(pose.theta_) * config_.scale_);

  SDL_SetRenderDrawColor(renderer_, 0xFF, 0xA5, 0x00, 0xFF); // orange color
  auto rays = player_.GetRays();
  for(const auto &ray : rays)
    {
      // DEBUG_MSG("FROM: " << pose.x_ * config_.scale_ << " - "
      //                    << pose.y_ * config_.scale_ << " TO: "
      //                    << ray.direction_.x * ray.length_ * config_.scale_
      //                    << " - "
      //                    << ray.direction_.y * ray.length_ *
      //                    config_.scale_);

      SDL_RenderDrawLine(
          renderer_, pose.x_ * config_.scale_, pose.y_ * config_.scale_,
          pose.x_ * config_.scale_
              + ray.direction_.x * ray.length_ * config_.scale_,
          pose.y_ * config_.scale_
              + ray.direction_.y * ray.length_ * config_.scale_);
    }
}

void
Game::Render3D()
{
  // DEBUG_MSG("Not initialized yet!!");
  Pose2D pose{ player_.GetPose() };
  auto rays = player_.GetRays();
  auto start_x = 0;
  for(const auto &ray : rays)
    {
      if(ray.length_ != 0.0)
        {
          if(ray.wall_color_ == 1)
            {
              SDL_SetRenderDrawColor(renderer_, 0xFF, 0x00, 0x00, 0xFF); // red
            }
          else if(ray.wall_color_ == 2)
            {
              SDL_SetRenderDrawColor(renderer_, 0x00, 0xFF, 0x00,
                                     0xFF); // green
            }
          else if(ray.wall_color_ == 3)
            {
              SDL_SetRenderDrawColor(renderer_, 0x00, 0x00, 0xFF,
                                     0xFF); // blue
            }
          else if(ray.wall_color_ == 4)
            {
              SDL_SetRenderDrawColor(renderer_, 0xFF, 0xFF, 0x00,
                                     0xFF); // yellow
            }
          else
            {
              SDL_SetRenderDrawColor(renderer_, 0xFF, 0xFF, 0xFF,
                                     0xFF); // white
            }

          // Calculate height of line to draw on screen
          auto ray_length
              = ray.perp_wall_dist_ * std::cos(pose.theta_ - ray.angle_);
          int lineHeight = (int)(config_.height_ / ray_length);

          // calculate lowest and highest pixel to fill in current stripe
          int drawStart = -lineHeight / 2 + config_.height_ / 2;
          if(drawStart < 0)
            drawStart = 0;
          int drawEnd = lineHeight / 2 + config_.height_ / 2;
          if(drawEnd >= config_.height_)
            drawEnd = config_.height_ - 1;

          if(!ray.is_x_side_)
            {
              SDL_SetRenderDrawColor(renderer_, 0xFF, 0xFF, 0xFF,
                                     0x80); // white
            }

          SDL_RenderDrawLine(renderer_, start_x * 2, drawStart, start_x * 2,
                             drawEnd);
        }
      start_x++;
    }
}

void
Game::RenderTextFPS()
{
  auto fps = clock_.GetFramePerSecond();
  SDL_Color color = { 255, 255, 255, 255 };
  std::string fpsString = "FPS: " + std::to_string(fps).substr(0, 5);
  TTF_Font *font = TTF_OpenFont(
      "/home/bilal/Desktop/wolfenstein/resources/font/Roboto-Light.ttf", 16);
  SDL_Surface *fpsSurface
      = TTF_RenderText_Solid(font, fpsString.c_str(), color);
  SDL_Texture *fpsTexture
      = SDL_CreateTextureFromSurface(renderer_, fpsSurface);
  SDL_Rect fpsRect = { 0, 0, fpsSurface->w, fpsSurface->h };
  SDL_RenderCopy(renderer_, fpsTexture, NULL, &fpsRect);
  SDL_FreeSurface(fpsSurface);
  SDL_DestroyTexture(fpsTexture);
}

void
Game::RenderFooter()
{
  SDL_Rect footer = { 10, 620, 1004, 138 };
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  SDL_RenderFillRect(renderer_, &footer);
}

void
Game::Clean()
{
  SDL_DestroyRenderer(renderer_);
  SDL_DestroyWindow(window_);
  SDL_Quit();
}

void
Game::Run()
{
  Init();
  while(isRunning_)
    {
      clock_.CalculateDeltaTime();
      CheckEvent();
      player_.Update(clock_.GetDeltaTime());
      Render();
      clock_.SleepForHz(config_.fps_);
    }
  DEBUG_MSG("Game is closing...");
}

} // namespace wolfenstein
