#include "Camera/camera.h"
#include "Characters/player.h"
#include "CollisionManager/collision_manager.h"
#include "Math/vector.h"
#include "SoundManager/sound_manager.h"
#include "State/weapon_state.h"
#include "Utility/uuid_generator.h"
#include <SDL2/SDL.h>
#include <memory>

namespace wolfenstein {

Player::Player(CharacterConfig& config, std::shared_ptr<Camera2D>& camera)
	: rotation_speed_(config.rotation_speed),
	  translation_speed_(config.translation_speed),
	  width_(config.width),
	  height_(config.height),
	  health_(100) {
	id_ = UuidGenerator::GetInstance().GenerateUuid().bytes();
	camera_ = camera;
	weapon_ = std::make_shared<Weapon>("mp5");
	weapon_->Init();
	position_ptr_ = std::make_shared<Position2D>(config.initial_position);
	damage_animation_ptr_ = std::make_unique<TriggeredSingleAnimation>(9, 1);
}

void Player::Update(double delta_time) {

	[this](double delta_time) {
		static double time = 0;
		time += delta_time;
		if (time >= 1.0) {
			time = 0.0;
			IncreaseHealth(1);
		}
	}(delta_time);
	if (!is_alive_) {
		return;
	}
	ShootOrReload();
	weapon_->Update(delta_time);
	Move(delta_time);
	Rotate(delta_time);
	camera_->Update();
	damage_animation_ptr_->Update(delta_time);
}

void Player::SetWeapon(std::shared_ptr<Weapon> weapon) {
	weapon_ = weapon;
	weapon_->SetCrossHair(camera_->GetCrosshairRay());
}

void Player::SetPose(const vector2d& pose) {
	position_ptr_->pose = pose;
}

vector2d Player::GetPose() const {
	return position_ptr_->pose;
}

ObjectType Player::GetObjectType() const {
	return ObjectType::CHARACTER_PLAYER;
}

void Player::SetPosition(const Position2D position) {
	*position_ptr_ = position;
}

void Player::IncreaseHealth(double amount) {
	health_ += amount;
	health_ = std::min(health_, 100.0);
}

void Player::DecreaseHealth(double amount) {
	health_ -= amount;
	if (health_ <= 0.0) {
		is_alive_ = false;
	}
	SoundManager::GetInstance().PlayEffect(id_, "player_pain");
	damaged_ = true;
	damage_animation_ptr_->Reset();
}

double Player::GetHealth() const {
	return health_;
}

Position2D Player::GetPosition() const {
	return *position_ptr_;
}

std::string Player::GetId() const {
	return id_;
}

int Player::GetTextureId() const {
	return weapon_->GetTextureId();
}

int Player::GetDamageTextureId() const {
	return damage_animation_ptr_->GetCurrentFrame();
};

double Player::GetWidth() const {
	return width_;
}
double Player::GetHeight() const {
	return height_;
}

const Ray& Player::GetCrosshairRay() const {
	return weapon_->GetCrosshair();
}

bool Player::IsDamaged() const {
	return damaged_;
}

bool Player::IsAlive() const {
	return is_alive_;
}

const Weapon& Player::GetWeapon() const {
	return *weapon_;
}

const std::shared_ptr<Position2D>& Player::GetPositionPtr() {
	return position_ptr_;
}
void Player::Move(double delta_time) {
	std::pair<double, double> delta_movement = {0.0, 0.0};
	double speed = translation_speed_ * delta_time;
	double speed_sin = speed * std::sin(position_ptr_->theta);
	double speed_cos = speed * std::cos(position_ptr_->theta);
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
			position_ptr_->pose, {delta_movement.first, 0})) {
		position_ptr_->pose.x += delta_movement.first;
	}
	if (!CollisionManager::GetInstance().CheckWallCollision(
			position_ptr_->pose, {0, delta_movement.second})) {
		position_ptr_->pose.y += delta_movement.second;
	}
}

void Player::Rotate(double delta_time) {
	if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
		int x, y;
		SDL_GetMouseState(&x, &y);
		position_ptr_->theta = SumRadian(
			position_ptr_->theta, (x - 400) * rotation_speed_ * delta_time);
		if (position_ptr_->theta > M_PI) {
			position_ptr_->theta -= 2 * M_PI;
		}
		else if (position_ptr_->theta < -M_PI) {
			position_ptr_->theta += 2 * M_PI;
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
