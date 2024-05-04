#include "include/player.h"
#include <SDL2/SDL.h>
#include <cmath>
#include "include/macros.h"

namespace wolfenstein {

Player::Player(const PlayerConfig& config)
    : pose(config.initial_pose_),
      player_speed_(config.player_speed_),
      player_rot_speed_(config.player_rot_speed_) {
  ray_caster_ = std::make_shared<RayCaster>(512, 10.0, 1.57079633);
}

void Player::Movement(const double delta_time) {
  double dx = 0.0;
  double dy = 0.0;
  auto speed = player_speed_ * delta_time;
  double speed_sin = speed * std::sin(pose.theta_);
  double speed_cos = speed * std::cos(pose.theta_);
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
    pose.theta_ -= player_rot_speed_ * delta_time;
  }
  if (keystate[SDL_SCANCODE_RIGHT]) {
    pose.theta_ += player_rot_speed_ * delta_time;
  }
  pose.theta_ = std::fmod(pose.theta_, (M_PI * 2));
}

void Player::Update(const double delta_time) {
  Movement(delta_time);
  ray_ = ray_caster_->Cast(pose);
}

Pose2D Player::GetPose() const {
  return pose;
}

vector2i Player::GetMapPose() const {
  return vector2i{static_cast<int>(pose.x_), static_cast<int>(pose.y_)};
}

std::vector<Ray> Player::GetRays() const {
  return ray_;
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
  if (!check_collision(static_cast<int>(pose.x_ + dx), pose.y_)) {
    pose.x_ += dx;
  }

  if (!check_collision(pose.x_, static_cast<int>(pose.y_ + dy))) {
    pose.y_ += dy;
  }
}

}  // namespace wolfenstein
