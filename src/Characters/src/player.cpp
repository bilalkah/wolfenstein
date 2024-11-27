#include "Characters/player.h"
#include "Camera/camera.h"
#include "CollisionManager/collision_manager.h"
#include "Math/vector.h"
#include "State/weapon_state.h"
#include "Utility/uuid_generator.h"
#include <SDL2/SDL.h>
#include <memory>

namespace wolfenstein {

Player::Player(CharacterConfig& config, std::shared_ptr<Camera2D>& camera)
	: position_(config.initial_position),
	  rotation_speed_(config.rotation_speed),
	  translation_speed_(config.translation_speed) {
	id_ = UuidGenerator::GetInstance().GenerateUuid().bytes();
	camera_ = camera;
	weapon_ = std::make_shared<Weapon>("mp5");
	WeaponStatePtr loaded_state = std::make_shared<LoadedState>();
	weapon_->TransitionTo(loaded_state);
}

void Player::Update(double delta_time) {
	ShootOrReload();
	weapon_->Update(delta_time);
	Move(delta_time);
	Rotate(delta_time);
	for (auto& subscriber : player_position_subscribers_) {
		subscriber(position_);
	}
	camera_->Update();
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

void Player::IncreaseHealth(double amount) {
	health_ += amount;
}

void Player::DecreaseHealth(double amount) {
	health_ -= amount;
}

double Player::GetHealth() const {
	return health_;
}

Position2D Player::GetPosition() const {
	return position_;
}

std::string Player::GetId() const {
	return id_;
}

int Player::GetTextureId() const {
	return weapon_->GetTextureId();
}
double Player::GetWidth() const {
	return width_;
}
double Player::GetHeight() const {
	return height_;
}

std::shared_ptr<Ray> Player::GetCrosshairRay() const {
	return camera_->GetCrosshairRay();
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

void Player::ShootOrReload() {
	// If R is pressed, reload
	const Uint8* keystate = SDL_GetKeyboardState(NULL);
	if (keystate[SDL_SCANCODE_R]) {
		weapon_->Reload();
	}

	// If left mouse button is pressed, attack
	if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) {
		weapon_->Attack();
	}
}

}  // namespace wolfenstein
