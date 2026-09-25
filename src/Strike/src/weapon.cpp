#include "Strike/weapon.h"
#include "State/weapon_state.h"
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

void Weapon::Charge() {
	ammo_ = config_.ammo_capacity;
}

void Weapon::Reload() {
	if (state_machine_.Current().GetType() != WeaponStateType::Reloading) {
		TransitionTo(WeaponStateType::Reloading);
	}
}

void Weapon::TransitionTo(WeaponStateType type) {
	state_machine_.TransitionTo(StateFor(type));
}

void Weapon::SetAmmo(size_t ammo) {
	ammo_ = ammo;
}

void Weapon::IncreaseAmmo() {
	ammo_++;
}

void Weapon::IncreaseAmmo(size_t amount) {
	ammo_ += amount;
}

void Weapon::DecreaseAmmo() {
	ammo_--;
}

void Weapon::DecreaseAmmo(size_t amount) {
	ammo_ -= amount;
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