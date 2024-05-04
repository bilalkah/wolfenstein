#include "include/player.h"
#include <SDL2/SDL.h>
#include <cmath>
#include "include/macros.h"

namespace wolfenstein {

Player::Player(const PlayerConfig& config) : config_(config) {
  ray_caster_ = std::make_shared<RayCaster>(512, 10.0, 1.57079633);
}

void Player::Movement(const double delta_time) {
  double dx = 0.0;
  double dy = 0.0;
  auto speed = config_.player_speed_ * delta_time;
  double speed_sin = speed * std::sin(config_.pose_.theta_);
  double speed_cos = speed * std::cos(config_.pose_.theta_);
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

  if (keystate[SDL_SCANCODE_LEFT]) {
    config_.pose_.theta_ -= config_.player_rot_speed_ * delta_time;
  }
  if (keystate[SDL_SCANCODE_RIGHT]) {
    config_.pose_.theta_ += config_.player_rot_speed_ * delta_time;
  }
  config_.pose_.theta_ = std::fmod(config_.pose_.theta_, (M_PI * 2));
}

void Player::Update(const double delta_time) {
  Movement(delta_time);
  rays_ = ray_caster_->Cast(config_.pose_);
}

Pose2D Player::GetPose() const {
  return config_.pose_;
}

vector2i Player::GetMapPose() const {
  return vector2i{static_cast<int>(config_.pose_.x_),
                  static_cast<int>(config_.pose_.y_)};
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
  if (!check_collision(static_cast<int>(config_.pose_.x_ + dx),
                       config_.pose_.y_)) {
    config_.pose_.x_ += dx;
  }

  if (!check_collision(config_.pose_.x_,
                       static_cast<int>(config_.pose_.y_ + dy))) {
    config_.pose_.y_ += dy;
  }
}

}  // namespace wolfenstein
