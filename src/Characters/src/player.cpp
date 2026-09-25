#include "Characters/player.h"
#include "Camera/camera.h"
#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include "Math/vector.h"
#include "Profiler/profiler.h"
#include "Settings/settings.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "State/weapon_state.h"
#include <SDL2/SDL.h>
#include <memory>
#include <utility>

namespace wolfenstein {

Player::Player(CharacterConfig& config, std::shared_ptr<Camera2D>& camera,
			   std::shared_ptr<Weapon> weapon, SoundManager& sound)
	: translation_speed_(config.translation_speed),
	  width_(config.width),
	  height_(config.height),
	  health_(100),
	  sound_(sound),
	  sound_channel_(sound.AllocateChannel()),
	  damage_animation_(9, 1) {
	camera_ = camera;
	position_ptr_ = std::make_shared<Position2D>(config.initial_position);
	SetWeapon(std::move(weapon));
}

void Player::Update(double delta_time) {

	// One health point per second; a member, not a function-local static,
	// so a new game does not inherit the last one's timer
	regen_time_ += delta_time;
	if (regen_time_ >= 1.0) {
		regen_time_ = 0.0;
		IncreaseHealth(1);
	}
	if (!is_alive_) {
		return;
	}
	ShootOrReload();
	weapon_->Update(delta_time);
	Move(delta_time);
	Rotate(delta_time);
	{
		ScopedTimer timer(ProfileSection::Camera);
		camera_->Update();
	}
	damage_animation_.Update(delta_time);
}

void Player::SetWeapon(std::shared_ptr<Weapon> weapon) {
	weapon_ = std::move(weapon);
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
	sound_.PlayEffect(sound_channel_, SoundEffect::PlayerPain);
	damaged_ = true;
	damage_animation_.Reset();
}

double Player::GetHealth() const {
	return health_;
}

Position2D Player::GetPosition() const {
	return *position_ptr_;
}

int Player::GetTextureId() const {
	return weapon_->GetTextureId();
}

int Player::GetDamageTextureId() const {
	return damage_animation_.GetCurrentFrame();
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
	const Map& map = scene_->GetMap();
	if (!CheckWallCollision(map, position_ptr_->pose,
							{delta_movement.first, 0})) {
		position_ptr_->pose.x += delta_movement.first;
	}
	if (!CheckWallCollision(map, position_ptr_->pose,
							{0, delta_movement.second})) {
		position_ptr_->pose.y += delta_movement.second;
	}
}

void Player::Rotate(double delta_time) {
	constexpr double kKeyboardTurnSpeed = 2.5;	// rad/s
	const Uint8* keystate = SDL_GetKeyboardState(NULL);
	double turn = 0.0;
	if (keystate[SDL_SCANCODE_LEFT]) {
		turn -= kKeyboardTurnSpeed * delta_time;
	}
	if (keystate[SDL_SCANCODE_RIGHT]) {
		turn += kKeyboardTurnSpeed * delta_time;
	}

	// Relative mouse mode reports motion since the last call, which also
	// works under browser pointer lock (unlike warping the cursor). Mouse
	// motion is already a distance, so it is not scaled by the frame time.
	constexpr double kRadiansPerPixel = 0.005;
	int dx = 0;
	SDL_GetRelativeMouseState(&dx, nullptr);
	if (SDL_GetRelativeMouseMode()) {
		turn += dx * kRadiansPerPixel * Settings::Get().mouse_sensitivity;
	}

	if (turn != 0.0) {
		position_ptr_->theta = SumRadian(position_ptr_->theta, turn);
	}
}

void Player::ShootOrReload() {
	// If R is pressed, reload
	const Uint8* keystate = SDL_GetKeyboardState(NULL);
	if (keystate[SDL_SCANCODE_R]) {
		weapon_->Reload();
	}

	// If left mouse button or left ctrl is pressed, attack
	if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) ||
		keystate[SDL_SCANCODE_LCTRL]) {
		if (weapon_->Attack()) {
			ResolvePlayerShot(*scene_, *weapon_);
		}
	}
}

}  // namespace wolfenstein
