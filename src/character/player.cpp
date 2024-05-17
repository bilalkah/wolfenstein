#include "character/player.h"
#include <SDL2/SDL.h>
#include <memory>

namespace wolfenstein {

Player::Player(const PlayerConfig config)
	: position_(config.initial_pose),
	  rotation_speed_(config.rotation_speed),
	  translation_speed_(config.translation_speed) {}

void Player::Update(const double delta_time) {
	delta_time_ = delta_time;
	Move();
	Rotate();
	camera_->Update(position_);
}

Position2D Player::GetPosition() const {
	return position_;
}

std::shared_ptr<RayVector> Player::GetRays() const {
	return camera_->GetRays();
}

void Player::SetCamera(std::unique_ptr<Camera2D> camera) {
	camera_ = std::move(camera);
}

std::shared_ptr<Ray> Player::GetCrosshairRay() const {
	return camera_->GetCrosshairRay();
}

void Player::Move() {
	double dx = 0.0;
	double dy = 0.0;
	double speed = translation_speed_ * delta_time_;
	double speed_sin = speed * std::sin(position_.theta);
	double speed_cos = speed * std::cos(position_.theta);
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

	const auto check_collision = [this](const auto dx, const auto dy) {
		constexpr double kCollisionDistance = 0.2;
		auto px = position_.pose.x;
		auto py = position_.pose.y;
		if ( dx > 0)
		{
			px += kCollisionDistance;
		}
		else if (dx < 0)
		{
			px -= kCollisionDistance;
		}
		if (dy > 0)
		{
			py += kCollisionDistance;
		}
		else if (dy < 0)
		{
			py -= kCollisionDistance;
		}
		return camera_->GetMap()->GetMap()[px][py] != 0;
	};

	if (!check_collision(dx, 0)) {
		position_.pose.x += dx;
	}
	if (!check_collision(0, dy)) {
		position_.pose.y += dy;
	}
}

void Player::Rotate() {
	if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
		int x, y;
		SDL_GetMouseState(&x, &y);
		position_.theta += (x - 400) * 0.001;
		SDL_WarpMouseInWindow(nullptr, 400, 300);
		position_.theta = std::fmod(position_.theta, (M_PI * 2));
	}
}

}  // namespace wolfenstein