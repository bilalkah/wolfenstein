#include "Strike/weapon.h"
#include "State/weapon_state.h"
#include "TimeManager/time_manager.h"
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
namespace wolfenstein {

namespace {
auto GetWeaponConfig = [](const std::string& weapon_name) -> WeaponConfig {
	if (weapon_name == "mp5") {
		return {"mp5", 18, 25, 2, 8, 0.5, 1.5};
	}
	else if (weapon_name == "shotgun") {
		return {"shotgun", 2, 60, 2, 7, 0.7, 3.5};
	}
	else {
		throw std::invalid_argument("Invalid weapon name");
	}
};
}  // namespace

Weapon::Weapon(std::string weapon_name)
	: weapon_properties_(GetWeaponConfig(weapon_name)),
	  sound_channel_(SoundManager::GetInstance().AllocateChannel()) {
	ammo_ = weapon_properties_.ammo_capacity;
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

void Weapon::Attack() {
	state_machine_.Current().PullTrigger();
}

void Weapon::Update(double delta_time) {
	state_machine_.Update(delta_time);
}

void Weapon::Charge() {
	ammo_ = weapon_properties_.ammo_capacity;
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

void Weapon::SetCrossHair(const std::shared_ptr<Ray> crosshair) {
	crosshair_ = crosshair;
}

size_t Weapon::GetAmmo() const {
	return ammo_;
}

size_t Weapon::GetAmmoCapacity() const {
	return weapon_properties_.ammo_capacity;
}

std::pair<double, double> Weapon::GetAttackDamage() const {
	return weapon_properties_.attack_damage;
}

double Weapon::GetAttackRange() const {
	return weapon_properties_.attack_range;
}

double Weapon::GetAttackSpeed() const {
	return weapon_properties_.attack_speed;
}

double Weapon::GetReloadSpeed() const {
	return weapon_properties_.reload_speed;
}

const std::string& Weapon::GetWeaponName() const {
	return weapon_properties_.weapon_name;
}

int Weapon::GetTextureId() const {
	return state_machine_.Current().GetCurrentFrame();
}

const Ray& Weapon::GetCrosshair() const {
	return *crosshair_;
}

}  // namespace wolfenstein