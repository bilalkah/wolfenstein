#include "include/player.h"
#include <SDL2/SDL.h>
#include <SDL_mouse.h>
#include <SDL_stdinc.h>
#include <cmath>
#include "types.h"
#include "utils.h"

namespace wolfenstein {

Player::Player(const PlayerConfig& config, uint16_t half_of_width,
               double view_distance, double fov)
    : config_(config) {
  ray_caster_ = std::make_shared<RayCaster>(half_of_width, view_distance, fov);
}

void Player::Movement(const double delta_time) {
  double dx = 0.0;
  double dy = 0.0;
  auto speed = config_.player_speed_ * delta_time;
  double speed_sin = speed * std::sin(config_.position_.theta);
  double speed_cos = speed * std::cos(config_.position_.theta);
  const Uint8* keystate = SDL_GetKeyboardState(NULL);

  if (keystate[SDL_SCANCODE_W]) {
    dx += speed_cos;
    dy += speed_sin;
  }
  if (keystate[SDL_SCANCODE_A]) {
    dx += speed_sin;
    dy += -speed_cos;
  }
  if (keystate[SDL_SCANCODE_S]) {
    dx += -speed_cos;
    dy += -speed_sin;
  }
  if (keystate[SDL_SCANCODE_D]) {
    dx += -speed_sin;
    dy += speed_cos;
  }

  MoveIfNotCollision(dx, dy);

  // check if cursor is visible
  if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
    int x, y;
    SDL_GetMouseState(&x, &y);
    config_.position_.theta += (x - 400) * 0.001;
    SDL_WarpMouseInWindow(nullptr, 400, 300);
    config_.position_.theta = std::fmod(config_.position_.theta, (M_PI * 2));
  }
}

void Player::Update(const double delta_time) {
  Movement(delta_time);
  rays_ = ray_caster_->Cast(config_.position_);
}

Position2D Player::GetPosition() const {
  return config_.position_;
}

vector2i Player::GetMapPose() const {
  return utility::ToVector2i(config_.position_.pose);
}

std::vector<Ray> Player::GetRays() const {
  return rays_;
}

void Player::SetMap(const std::shared_ptr<Map>& map) {
  map_ptr_ = map;
  ray_caster_->SetMap(map);
}

void Player::MoveIfNotCollision(double dx, double dy) {
  auto check_collision = [this](const auto& x, const auto& y) {
    bool is_collide = map_ptr_->GetMap()[x][y] != 0;
    return is_collide;
  };

  if (!check_collision(static_cast<int>(config_.position_.pose.x + dx),
                       config_.position_.pose.y)) {
    config_.position_.pose.x += dx;
  }

  if (!check_collision(config_.position_.pose.x,
                       static_cast<int>(config_.position_.pose.y + dy))) {
    config_.position_.pose.y += dy;
  }
}

}  // namespace wolfenstein
