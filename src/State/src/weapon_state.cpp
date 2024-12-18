#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "State/state.h"
#include "State/weapon_state.h"
#include "Strike/weapon.h"
#include <memory>

namespace wolfenstein {

void WeaponState::Reset() {
	animation_->Reset();
}

int WeaponState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

// ########################################### LoadedState ###########################################
LoadedState::LoadedState()
	: trigger_pulled_(false), trigger_pull_time_(0.0), fire_rate_(0.0) {}

LoadedState::~LoadedState() {}

void LoadedState::Update(const double& delta_time) {
	if (trigger_pulled_) {
		animation_->Update(delta_time);
		trigger_pull_time_ += delta_time;
		if (trigger_pull_time_ >= fire_rate_) {
			trigger_pulled_ = false;
			animation_->Reset();
			if (context_->GetAmmo() == 0) {
				context_->TransitionTo(std::make_shared<OutOfAmmoState>());
				return;
			}
		}
	}
}

WeaponStateType LoadedState::GetType() const {
	return WeaponStateType::Loaded;
}

void LoadedState::OnContextSet() {
	fire_rate_ = context_->GetAttackSpeed();
	animation_ = std::make_unique<LoopedAnimation>(
		context_->GetWeaponName() + "_loaded", fire_rate_);
}

void LoadedState::PullTrigger() {
	if (trigger_pulled_) {
		return;
	}
	SoundManager::GetInstance().PlayEffect("weapon", "shotgun");
	trigger_pulled_ = true;
	trigger_pull_time_ = 0;
	context_->DecreaseAmmo();
	ShootingManager::GetInstance().PlayerShoot(*context_);
}

// ########################################### OutOfAmmoState ###########################################
OutOfAmmoState::OutOfAmmoState()
	: trigger_pulled_(false), trigger_pull_time_(0.0), fire_rate_(0.0) {}

OutOfAmmoState::~OutOfAmmoState() {}

void OutOfAmmoState::Update(const double& delta_time) {
	if (trigger_pulled_) {
		animation_->Update(delta_time);
		trigger_pull_time_ += delta_time;
		if (trigger_pull_time_ >= fire_rate_) {
			trigger_pulled_ = false;
			animation_->Reset();
		}
	}
}

WeaponStateType OutOfAmmoState::GetType() const {
	return WeaponStateType::OutOfAmmo;
}

void OutOfAmmoState::OnContextSet() {
	fire_rate_ = context_->GetAttackSpeed();
	animation_ = std::make_unique<LoopedAnimation>(
		context_->GetWeaponName() + "_outofammo", fire_rate_);
}

void OutOfAmmoState::PullTrigger() {
	if (trigger_pulled_) {
		return;
	}
	trigger_pulled_ = true;
	trigger_pull_time_ = 0;
}

// ########################################### ReloadingState ###########################################
ReloadingState::ReloadingState() : reload_time_(0.0) {}

ReloadingState::~ReloadingState() {}

void ReloadingState::Update(const double& delta_time) {
	animation_->Update(delta_time);
	reload_time_ += delta_time;
	if (reload_time_ >= reload_speed_) {
		animation_->Reset();
		context_->SetAmmo(context_->GetAmmoCapacity());
		context_->TransitionTo(std::make_shared<LoadedState>());
		return;
	}
}

WeaponStateType ReloadingState::GetType() const {
	return WeaponStateType::Reloading;
}

void ReloadingState::OnContextSet() {
	reload_speed_ = context_->GetReloadSpeed();
	animation_ = std::make_unique<LoopedAnimation>(
		context_->GetWeaponName() + "_reload", reload_speed_);
}

}  // namespace wolfenstein
