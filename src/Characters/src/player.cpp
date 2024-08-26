#include "Characters/player.h"
#include "CollisionManager/collision_manager.h"
#include "Math/vector.h"
#include "Utility/uuid_generator.h"
#include <SDL2/SDL.h>

namespace wolfenstein {

Player::Player(CharacterConfig& config)
	: position_(config.initial_position),
	  rotation_speed_(config.rotation_speed),
	  translation_speed_(config.translation_speed) {
	id_ = UuidGenerator::GetInstance().GenerateUuid().bytes();
}

void Player::Update(double delta_time) {
	Move(delta_time);
	Rotate(delta_time);
	for (auto& subscriber : player_position_subscribers_) {
		subscriber(position_);
	}
}

void Player::SetPose(const vector2d& pose) {
	position_.pose = pose;
}

vector2d Player::GetPose() const {
	return position_.pose;
}

ObjectType Player::GetObjectType() const {
	return ObjectType::CHARACTER_PLAYER;
}

void Player::SetPosition(Position2D position) {
	position_ = position;
}

Position2D Player::GetPosition() const {
	return position_;
}

std::string Player::GetId() const {
	return id_;
}

void Player::SubscribeToPlayerPosition(
	std::function<void(Position2D)> updator) {
	player_position_subscribers_.push_back(updator);
}

void Player::Move(double delta_time) {
	std::pair<double, double> delta_movement = {0.0, 0.0};
	double speed = translation_speed_ * delta_time;
	double speed_sin = speed * std::sin(position_.theta);
	double speed_cos = speed * std::cos(position_.theta);
	const Uint8* keystate = SDL_GetKeyboardState(NULL);

	if (keystate[SDL_SCANCODE_W]) {
		delta_movement.first += speed_cos;
		delta_movement.second += speed_sin;
	}
	if (keystate[SDL_SCANCODE_A]) {
		delta_movement.first += speed_sin;
		delta_movement.second -= speed_cos;
	}
	if (keystate[SDL_SCANCODE_S]) {
		delta_movement.first -= speed_cos;
		delta_movement.second -= speed_sin;
	}
	if (keystate[SDL_SCANCODE_D]) {
		delta_movement.first -= speed_sin;
		delta_movement.second += speed_cos;
	}
	if (!CollisionManager::GetInstance().CheckWallCollision(
			position_.pose, {delta_movement.first, 0})) {
		position_.pose.x += delta_movement.first;
	}
	if (!CollisionManager::GetInstance().CheckWallCollision(
			position_.pose, {0, delta_movement.second})) {
		position_.pose.y += delta_movement.second;
	}
}

void Player::Rotate(double delta_time) {
	if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
		int x, y;
		SDL_GetMouseState(&x, &y);
		position_.theta = SumRadian(position_.theta,
									(x - 400) * rotation_speed_ * delta_time);
		if (position_.theta > M_PI) {
			position_.theta -= 2 * M_PI;
		}
		else if (position_.theta < -M_PI) {
			position_.theta += 2 * M_PI;
		}
		SDL_WarpMouseInWindow(nullptr, 400, 300);
	}
}

}  // namespace wolfenstein
