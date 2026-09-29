#include "Characters/player.h"
#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include "Math/vector.h"
#include "Profiler/profiler.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "State/weapon_state.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <utility>

namespace wolfenstein {

Player::Player(CharacterConfig& config, std::span<const WeaponConfig> arsenal,
			   std::size_t first, const TextureManager& textures,
			   SoundManager& sound)
	: translation_speed_(config.translation_speed),
	  width_(config.width),
	  height_(config.height),
	  health_(100),
	  sound_(sound),
	  sound_channel_(sound.AllocateChannel()),
	  step_channel_(sound.AllocateChannel()),
	  position_(config.initial_position),
	  previous_position_(config.initial_position),
	  weapon_count_(std::min(arsenal.size(), kMaxWeapons)),
	  damage_animation_(1.0),
	  pickup_animation_(3.0, 80, 0) {
	for (std::size_t i = 0; i < weapon_count_; ++i) {
		weapons_[i].emplace(arsenal[i], textures, sound);
		if (arsenal[i].start) {
			owned_ |= static_cast<std::uint8_t>(1U << i);
		}
	}
	held_ = std::min(first, weapon_count_ - 1);
	owned_ |= static_cast<std::uint8_t>(1U << held_);
}

Player::Player(CharacterConfig& config, const WeaponConfig& weapon,
			   const TextureManager& textures, SoundManager& sound)
	: Player(config, std::span(&weapon, 1), 0, textures, sound) {}

const Weapon& Player::GetWeapon(std::size_t index) const {
	assert(index < weapon_count_);
	auto& weapon = weapons_[index];
	if (!weapon.has_value()) {
		std::
			unreachable();	// every one of the arsenal is built with the player
	}
	return *weapon;
}

Weapon& Player::GetWeapon(std::size_t index) {
	assert(index < weapon_count_);
	auto& weapon = weapons_[index];
	if (!weapon.has_value()) {
		std::
			unreachable();	// every one of the arsenal is built with the player
	}
	return *weapon;
}

void Player::SetOwnedWeapons(std::uint8_t owned) {
	const auto all = static_cast<std::uint8_t>((1U << weapon_count_) - 1);
	owned_ = static_cast<std::uint8_t>(owned & all);
	if (!Owns(held_)) {
		owned_ |= static_cast<std::uint8_t>(1U << held_);
	}
}

void Player::SelectWeapon(std::size_t index) {
	if (!Owns(index)) {
		return;
	}
	if (coming_) {
		// Changed its mind while the gun goes down: another comes up, or
		// the one going down comes back
		if (index == held_) {
			coming_.reset();
			GetWeapon(held_).TransitionTo(WeaponStateType::Raising);
		}
		else {
			coming_ = index;
		}
		return;
	}
	if (index == held_) {
		return;
	}
	coming_ = index;
	GetWeapon(held_).TransitionTo(WeaponStateType::Lowering);
}

void Player::TakeInHand(std::size_t index) {
	if (!Owns(index)) {
		return;
	}
	coming_.reset();
	held_ = index;
	GetWeapon(held_).TransitionTo(WeaponStateType::Loaded);
}

void Player::Update(double delta_time) {
	previous_position_ = position_;
	previous_kick_ = kick_;
	// The view settles back after a shot's kick within a tenth of a second
	constexpr double kKickSettling = 30.0;	// per second
	kick_ *= std::exp(-kKickSettling * delta_time);
	pickup_animation_.Update(delta_time);
	since_hurt_ += delta_time;
	since_hit_ += delta_time;
	if (puppet_) {
		since_death_ += is_alive_ ? 0.0 : delta_time;
		return;	 // placed from outside (Follow), after this
	}
	if (!is_alive_) {
		// Falling, and a thud as the body lands
		const bool falling = since_death_ < kFallSeconds;
		since_death_ += delta_time;
		if (falling && since_death_ >= kFallSeconds) {
			sound_.PlayEffect(sound_channel_, SoundEffect::PlayerFall);
		}
		return;
	}
	SwitchWeapons();
	ShootOrReload();
	GetWeapon(held_).Update(delta_time);
	// The gun in hand is down: the next comes up, afresh, not mid-shot or
	// mid-reload from the last time it was held
	if (const auto next = coming_; next && GetWeapon(held_).IsDown()) {
		held_ = *next;
		coming_.reset();
		GetWeapon(held_).TransitionTo(WeaponStateType::Raising);
	}
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
	pitch_ = 0.0;  // a new place: looking straight ahead
	// A teleport, not a move: nothing to interpolate across
	previous_position_ = position;
}

void Player::IncreaseHealth(double amount) {
	health_ += amount;
	health_ = std::min(health_, 100.0);
}

void Player::DecreaseHealth(double amount) {
	if (!is_alive_) {
		return;	 // fallen: nothing more hurts it
	}
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
	return GetWeapon(held_).GetTextureId();
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
	// An ammo box tops up every firearm carried
	if (effect.ammo_boxes > 0) {
		for (std::size_t i = 0; i < weapon_count_; ++i) {
			if (Owns(i) &&
				GetWeapon(i).AddAmmoBoxes(effect.ammo_boxes,
										  supplies * effect.box_share)) {
				taken = true;
			}
		}
	}
	// A weapon found is taken in hand; one already carried gives a box of
	// its rounds instead
	for (std::size_t i = 0; i < weapon_count_; ++i) {
		if ((effect.weapons >> i & 1U) == 0) {
			continue;
		}
		if (!Owns(i)) {
			owned_ |= static_cast<std::uint8_t>(1U << i);
			SelectWeapon(i);
			taken = true;
		}
		else if (GetWeapon(i).AddAmmoBoxes(1, supplies)) {
			taken = true;
		}
	}
	if ((effect.keys & ~keys_) != 0) {
		keys_ |= effect.keys;
		taken = true;
	}
	if (taken) {
		// What it sounds like, by the most it gave: a gun, a key, rounds,
		// else health
		const SoundEffect sound =
			effect.weapons != 0 ? SoundEffect::WeaponPickup
			: effect.keys != 0	? SoundEffect::KeyPickup
			: effect.ammo_boxes > 0 && effect.health <= 0.0
				? SoundEffect::AmmoPickup
				: SoundEffect::Pickup;
		sound_.PlayEffect(sound_channel_, sound);
		picked_up_ = true;
		pickup_animation_.Reset();
	}
	return taken;
}

void Player::Restore(double health, std::size_t ammo, std::size_t reserve) {
	health_ = std::clamp(health, 1.0, 100.0);
	GetWeapon(held_).SetRounds(ammo, reserve);
}

double Player::GetHitMarker() const {
	constexpr double kShowSeconds = 0.2;
	return std::max(1.0 - since_hit_ / kShowSeconds, 0.0);
}

double Player::GetDeathFall() const {
	if (is_alive_) {
		return 0.0;
	}
	const double t = std::min(since_death_ / kFallSeconds, 1.0);
	return t * t;
}

double Player::GetEyeHeight() const {
	// Half a wall up, down to the floor but for a head's height
	constexpr double kStanding = 0.5;
	constexpr double kLying = 0.08;
	return kStanding - (kStanding - kLying) * GetDeathFall();
}

bool Player::IsDamaged() const {
	return damaged_;
}

bool Player::IsAlive() const {
	return is_alive_;
}

const Weapon& Player::GetWeapon() const {
	return GetWeapon(held_);
}

void Player::SetCommand(const PlayerCommand& command) {
	command_ = command;
}

void Player::Follow(const Position2D& position, double pitch, double health,
					bool alive) {
	if (alive && !is_alive_) {
		// Back in the game: where it comes in, not drawn sliding there
		is_alive_ = true;
		since_death_ = 0.0;
		previous_position_ = position;
	}
	else if (!alive && is_alive_) {
		is_alive_ = false;
		since_death_ = 0.0;
	}
	position_ = position;
	pitch_ = pitch;
	health_ = health;
}

void Player::Replay(const PlayerCommand& command, double delta_time) {
	command_ = command;
	replaying_ = true;
	Move(delta_time);
	Rotate(delta_time);
	replaying_ = false;
}

void Player::Correct(const Position2D& position) {
	position_ = position;
}

void Player::Move(double delta_time) {
	const double speed = translation_speed_ * delta_time;
	const vector2d facing{std::cos(position_.theta), std::sin(position_.theta)};
	const vector2d right{-facing.y, facing.x};
	// Forward and sideways at once, no faster than either alone
	vector2d wish = facing * command_.forward + right * command_.strafe;
	const double length = wish.Magnitude();
	if (length > 1.0) {
		wish = wish / length;
	}
	const vector2d delta_movement = wish * speed;
	// As far as it can go: out of the living enemies, lamps and other
	// players it meets (sliding round them), then an axis at a time against
	// the walls (sliding along them)
	const Map& map = scene_->GetMap();
	vector2d reached =
		ResolveObjectCollisions(scene_->GetObjects(), this, position_.pose,
								position_.pose + delta_movement, width_ / 2);
	for (const Player* other : scene_->GetPlayers()) {
		if (other != nullptr && other != this && other->IsAlive()) {
			reached = PushOutOf(other->GetPose(), other->GetWidth() / 2,
								position_.pose, reached, width_ / 2);
		}
	}
	const vector2d step = reached - position_.pose;
	const vector2d before = position_.pose;
	if (!CheckWallCollision(map, position_.pose, {step.x, 0}, width_ / 2)) {
		position_.pose.x += step.x;
	}
	if (!CheckWallCollision(map, position_.pose, {0, step.y}, width_ / 2)) {
		position_.pose.y += step.y;
	}
	// A footstep every stride walked, one foot then the other
	constexpr double kStride = 0.9;
	// A replay goes over steps already taken (and heard)
	if (replaying_) {
		return;
	}
	walked_ += position_.pose.Distance(before);
	if (walked_ >= kStride) {
		walked_ -= kStride;
		left_foot_ = !left_foot_;
		sound_.PlayEffect(step_channel_, left_foot_ ? SoundEffect::StepLeft
													: SoundEffect::StepRight);
	}
}

void Player::Rotate(double delta_time) {
	// Where to look, set outright: the game turned the view with the input
	if (command_.has_view) {
		position_.theta = command_.view_theta;
		pitch_ = std::clamp(command_.view_pitch, -kMaxPitch, kMaxPitch);
		return;
	}
	// Mouse motion is already a distance, so it is not scaled by the time
	// step, and it is spent by the first tick that applies it
	const double turn =
		command_.turn * kKeyboardTurnSpeed * delta_time + command_.look;
	command_.look = 0.0;
	if (turn != 0.0) {
		position_.theta = SumRadian(position_.theta, turn);
	}
	pitch_ = std::clamp(pitch_ + command_.look_up, -kMaxPitch, kMaxPitch);
	command_.look_up = 0.0;
}

// A number key takes that weapon in hand; the wheel steps through those
// carried, round and round
void Player::SwitchWeapons() {
	if (command_.weapon >= 0) {
		SelectWeapon(static_cast<std::size_t>(command_.weapon));
	}
	if (command_.cycle != 0) {
		const auto count = static_cast<int>(weapon_count_);
		// On from the weapon coming into hand, if one is
		int index = static_cast<int>(coming_.value_or(held_));
		for (int step = 0; step < count; ++step) {
			index = (index + command_.cycle + count) % count;
			if (Owns(static_cast<std::size_t>(index))) {
				SelectWeapon(static_cast<std::size_t>(index));
				break;
			}
		}
	}
	// Each is taken once, like mouse look
	command_.weapon = -1;
	command_.cycle = 0;
}

void Player::ShootOrReload() {
	Weapon& weapon = GetWeapon(held_);
	if (command_.reload) {
		weapon.Reload();
	}
	if (command_.fire && weapon.Attack()) {
		kick_ = std::max(kick_, weapon.GetKick());
		scene_->MakeNoise(position_.pose, weapon.GetNoiseRange());
		// A rocket or a bolt flies, and bursts on what it meets later
		if (const ProjectileConfig* projectile = weapon.GetProjectile()) {
			scene_->Launch(*projectile, position_.pose, position_.theta,
						   weapon.GetAttackDamage().first);
			return;
		}
		const ShotResult result =
			ResolvePlayerShot(*scene_, weapon, position_, pitch_);
		if (result.hit) {
			NoteHit(result.head);
			weapon.PlaySound(weapon.GetHitSound());
		}
	}
}

}  // namespace wolfenstein
