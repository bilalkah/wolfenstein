#include "Strike/weapon.h"
#include "State/weapon_state.h"
#include <exception>

namespace wolfenstein {

namespace {
auto GetWeaponConfig = [](const std::string& weapon_name) -> WeaponConfig {
	if (weapon_name == "mp5") {
		return {"mp5", 0.2, 10, 10};
	}
	else {
		return {"Doesn't exist", -1, -1, 0};
	}
};
}  // namespace

Weapon::Weapon(std::string weapon_name)
	: weapon_config_(GetWeaponConfig(weapon_name)),
	  cooldown_(false),
	  reloading_(false) {
	state_ = std::make_shared<LoadedState>(weapon_config_.weapon_name);
}

Weapon::~Weapon() {}

void Weapon::Init() {
	state_->SetContext(shared_from_this());
}

void Weapon::Attack() {
	state_->Update(0.0);
}

int Weapon::GetTextureId() const {
	return state_->GetCurrentFrame();
}

void Weapon::TransitionTo(std::shared_ptr<State<Weapon>> state) {
	state_ = state;
	state_->SetContext(shared_from_this());
}

void Weapon::decreaseAmmo() {
	weapon_config_.ammo_capacity--;
}

size_t Weapon::getAmmo() const {
	return weapon_config_.ammo_capacity;
}

std::string Weapon::getWeaponName() const {
	return weapon_config_.weapon_name;
}

void Weapon::Reload() {
	if (!reloading_) {
		reloading_ = true;
		TransitionTo(
			std::make_shared<ReloadingState>(weapon_config_.weapon_name));
		state_->Update(0.0);
	}
}

void Weapon::reloadFinished() {
	reloading_ = false;
	weapon_config_.ammo_capacity = 10;
}

}  // namespace wolfenstein