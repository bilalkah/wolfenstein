#include "Strike/weapon.h"
#include "State/weapon_state.h"
#include "TimeManager/time_manager.h"
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
namespace wolfenstein {

namespace {
auto GetWeaponConfig = [](const std::string& weapon_name) -> WeaponConfig {
	if (weapon_name == "mp5") {
		return {"mp5", 8, 15, 2, 8, 0.5, 1.5};
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
	: weapon_properties_(GetWeaponConfig(weapon_name)) {
	ammo_ = weapon_properties_.ammo_capacity;
	state_ = std::make_shared<LoadedState>();
}

Weapon::~Weapon() {}

void Weapon::Init() {
	state_->SetContext(shared_from_this());
}

void Weapon::Attack() {
	state_->PullTrigger();
}

void Weapon::Update(double delta_time) {
	state_->Update(delta_time);
}

void Weapon::Charge() {
	ammo_ = weapon_properties_.ammo_capacity;
}

void Weapon::Reload() {
	WeaponStatePtr reloading_state = std::make_shared<ReloadingState>();
	TransitionTo(reloading_state);
}

void Weapon::TransitionTo(WeaponStatePtr& state) {
	state_ = state;
	state_->SetContext(shared_from_this());
	if (state->GetType() == WeaponStateType::Reloading) {
		state_->Update(0.0);
	}
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

void Weapon::SetCrossHair(std::shared_ptr<Ray> crosshair) {
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

std::string Weapon::GetWeaponName() const {
	return weapon_properties_.weapon_name;
}

int Weapon::GetTextureId() const {
	return state_->GetCurrentFrame();
}

std::shared_ptr<Ray> Weapon::GetCrosshair() const {
	return crosshair_;
}

}  // namespace wolfenstein