#include "Strike/weapon.h"
#include "State/weapon_state.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
namespace wolfenstein {

Weapon::Weapon(const WeaponConfig& config, const TextureManager& textures,
			   SoundManager& sound)
	: textures_(textures),
	  sound_(sound),
	  config_(config),
	  sound_channel_(sound.AllocateChannel()) {
	ammo_ = config_.ammo_capacity;
	reserve_ = config_.reserve_start;
	for (const auto type : {WeaponStateType::Loaded, WeaponStateType::OutOfAmmo,
							WeaponStateType::Reloading}) {
		StateFor(type).SetContext(*this);
	}
	state_machine_.TransitionTo(loaded_state_);
}

WeaponState& Weapon::StateFor(WeaponStateType type) {
	switch (type) {
		case WeaponStateType::Loaded:
			return loaded_state_;
		case WeaponStateType::OutOfAmmo:
			return out_of_ammo_state_;
		case WeaponStateType::Reloading:
			return reloading_state_;
	}
	std::unreachable();
}

bool Weapon::Attack() {
	return state_machine_.Current().PullTrigger();
}

void Weapon::Update(double delta_time) {
	state_machine_.Update(delta_time);
}

void Weapon::Reload() {
	if (state_machine_.Current().GetType() != WeaponStateType::Reloading &&
		ammo_ < config_.ammo_capacity && reserve_ > 0) {
		TransitionTo(WeaponStateType::Reloading);
	}
}

void Weapon::FinishReload() {
	const size_t moved = std::min(config_.ammo_capacity - ammo_, reserve_);
	ammo_ += moved;
	reserve_ -= moved;
}

void Weapon::TransitionTo(WeaponStateType type) {
	state_machine_.TransitionTo(StateFor(type));
}

void Weapon::SetRounds(size_t ammo, size_t reserve) {
	ammo_ = std::min(ammo, config_.ammo_capacity);
	reserve_ = std::min(reserve, config_.reserve_max);
}

void Weapon::DecreaseAmmo() {
	ammo_--;
}

bool Weapon::AddAmmoBoxes(size_t boxes, double scale) {
	if (reserve_ >= config_.reserve_max) {
		return false;
	}
	const auto rounds = static_cast<size_t>(
		std::lround(static_cast<double>(boxes * config_.box_rounds) * scale));
	reserve_ = std::min(config_.reserve_max, reserve_ + rounds);
	return true;
}

size_t Weapon::GetAmmo() const {
	return ammo_;
}

size_t Weapon::GetAmmoCapacity() const {
	return config_.ammo_capacity;
}

std::pair<double, double> Weapon::GetAttackDamage() const {
	return config_.attack_damage;
}

double Weapon::GetAttackRange() const {
	return config_.attack_range;
}

double Weapon::GetAttackSpeed() const {
	return config_.attack_speed;
}

double Weapon::GetReloadSpeed() const {
	return config_.reload_speed;
}

const std::string& Weapon::GetWeaponName() const {
	return config_.weapon_name;
}

int Weapon::GetTextureId() const {
	return state_machine_.Current().GetCurrentFrame();
}

}  // namespace wolfenstein