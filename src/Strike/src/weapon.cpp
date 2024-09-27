#include "Strike/weapon.h"
#include "State/weapon_state.h"
#include <cstddef>
#include <stdexcept>
#include <string>
namespace wolfenstein {

namespace {
auto GetWeaponConfig = [](const std::string& weapon_name) -> WeaponConfig {
	if (weapon_name == "mp5") {
		return {"mp5", 10, 10, 10, 0.3, 1.5};
	}
	else {
		throw std::invalid_argument("Invalid weapon name");
	}
};
}  // namespace

Weapon::Weapon(std::string weapon_name)
	: weapon_properties_(GetWeaponConfig(weapon_name)),
	  ammo_(weapon_properties_.ammo_capacity),
	  state_(std::make_shared<WeaponState>()) {}

Weapon::~Weapon() {}

void Weapon::Init() {
	state_->SetContext(shared_from_this());
}

void Weapon::Attack() {
	state_->Update(0.0);
}

void Weapon::Charge() {
	ammo_ = weapon_properties_.ammo_capacity;
}

void Weapon::Reload() {
	state_->TransitionRequest(WeaponStateType::Reloading);
}

void Weapon::TransitionTo(WeaponStatePtr& state) {
	(*state_) = state;
	state_->SetContext(shared_from_this());
	if (state->GetType() == WeaponStateType::Reloading) {
		state_->Update(0.0);
	}
}

void Weapon::TransitionTo(WeaponStateType state_type) {
	(*state_) = state_type;
	state_->SetContext(shared_from_this());
	if (state_type == WeaponStateType::Reloading) {
		state_->Update(0.0);
	}
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
	return weapon_properties_.ammo_capacity;
}

double Weapon::GetAttackDamage() const {
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

}  // namespace wolfenstein