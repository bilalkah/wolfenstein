#include "Characters/player.h"
#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include "Math/vector.h"
#include "Profiler/profiler.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "State/weapon_state.h"
#include <algorithm>
#include <memory>
#include <utility>

namespace wolfenstein {

Player::Player(CharacterConfig& config, const WeaponConfig& weapon,
			   const TextureManager& textures, SoundManager& sound)
	: translation_speed_(config.translation_speed),
	  width_(config.width),
	  height_(config.height),
	  health_(100),
	  sound_(sound),
	  sound_channel_(sound.AllocateChannel()),
	  position_(config.initial_position),
	  previous_position_(config.initial_position),
	  weapon_(weapon, textures, sound),
	  damage_animation_(1.0),
	  pickup_animation_(3.0, 80, 0) {}

void Player::Update(double delta_time) {
	previous_position_ = position_;
	pickup_animation_.Update(delta_time);
	since_hurt_ += delta_time;
	if (!is_alive_) {
		return;
	}
	ShootOrReload();
	weapon_.Update(delta_time);
	Move(delta_time);
	Rotate(delta_time);
	damage_animation_.Update(delta_time);
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

void Player::SetPosition(const Position2D position) {
	position_ = position;
	// A teleport, not a move: nothing to interpolate across
	previous_position_ = position;
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
	since_hurt_ = 0.0;
	damaged_ = true;
	damage_animation_.Reset();
}

double Player::GetHealth() const {
	return health_;
}

Position2D Player::GetRenderPosition(double alpha) const {
	return Interpolate(previous_position_, position_, alpha);
}

int Player::GetTextureId() const {
	return weapon_.GetTextureId();
}

double Player::GetWidth() const {
	return width_;
}
double Player::GetHeight() const {
	return height_;
}

bool Player::TryPickUp(const PickupEffect& effect, double supplies) {
	bool taken = false;
	if (effect.health > 0.0 && health_ < 100.0) {
		IncreaseHealth(effect.health * supplies);
		taken = true;
	}
	if (effect.ammo_boxes > 0 &&
		weapon_.AddAmmoBoxes(effect.ammo_boxes, supplies)) {
		taken = true;
	}
	if ((effect.keys & ~keys_) != 0) {
		keys_ |= effect.keys;
		taken = true;
	}
	if (taken) {
		sound_.PlayEffect(sound_channel_, SoundEffect::Pickup);
		picked_up_ = true;
		pickup_animation_.Reset();
	}
	return taken;
}

void Player::Restore(double health, std::size_t ammo, std::size_t reserve) {
	health_ = std::clamp(health, 1.0, 100.0);
	weapon_.SetRounds(ammo, reserve);
}

bool Player::IsDamaged() const {
	return damaged_;
}

bool Player::IsAlive() const {
	return is_alive_;
}

const Weapon& Player::GetWeapon() const {
	return weapon_;
}

void Player::SetCommand(const PlayerCommand& command) {
	command_ = command;
}

void Player::Move(double delta_time) {
	const double speed = translation_speed_ * delta_time;
	const vector2d facing{std::cos(position_.theta), std::sin(position_.theta)};
	const vector2d right{-facing.y, facing.x};
	const vector2d delta_movement =
		facing * (command_.forward * speed) + right * (command_.strafe * speed);
	const Map& map = scene_->GetMap();
	if (!CheckWallCollision(map, position_.pose, {delta_movement.x, 0})) {
		position_.pose.x += delta_movement.x;
	}
	if (!CheckWallCollision(map, position_.pose, {0, delta_movement.y})) {
		position_.pose.y += delta_movement.y;
	}
}

void Player::Rotate(double delta_time) {
	constexpr double kKeyboardTurnSpeed = 2.5;	// rad/s
	// Mouse motion is already a distance, so it is not scaled by the time
	// step, and it is spent by the first tick that applies it
	const double turn =
		command_.turn * kKeyboardTurnSpeed * delta_time + command_.look;
	command_.look = 0.0;
	if (turn != 0.0) {
		position_.theta = SumRadian(position_.theta, turn);
	}
}

void Player::ShootOrReload() {
	if (command_.reload) {
		weapon_.Reload();
	}
	if (command_.fire && weapon_.Attack()) {
		ResolvePlayerShot(*scene_, weapon_, position_);
	}
}

}  // namespace wolfenstein
