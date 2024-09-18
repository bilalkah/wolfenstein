#include "State/weapon_state.h"
#include "State/state.h"
#include "Strike/weapon.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL2/SDL.h>
#include <memory>
#include <thread>
#include <unordered_map>

namespace wolfenstein {

namespace {

const std::unordered_map<std::string,
						 std::unordered_map<std::string, std::string>>
	weapon_data_{{"mp5",
				  {{"loaded", "mp5_loaded"},
				   {"outofammo", "mp5_outofammo"},
				   {"reload", "mp5_reload"}}}};

}

// ########################################### LoadedState ###########################################
LoadedState::LoadedState(const std::string weapon_name)
	: weapon_name_(weapon_name),
	  last_attack_time_(0),
	  cooldown_(false),
	  interrupt_(false),
	  destroyed_(false) {}

LoadedState::~LoadedState() {
	destroyed_ = true;
}

void LoadedState::Update(const double&) {
	if (!cooldown_) {
		cooldown_ = true;
		last_attack_time_ = TimeManager::GetInstance().GetCurrentTime();
		std::thread attack_thread(&LoadedState::AttackAnimation, this);
		attack_thread.detach();
	}
}

WeaponStateType LoadedState::GetType() const {
	return WeaponStateType::Loaded;
}

void LoadedState::AttackAnimation() {
	auto attack_time = last_attack_time_;
	auto current_time = TimeManager::GetInstance().GetCurrentTime();
	while (current_time - last_attack_time_ < animation_speed_ && !interrupt_ &&
		   !destroyed_) {
		auto time_elapsed = current_time - attack_time;
		animation_->Update(time_elapsed);
		attack_time = current_time;
		current_time = TimeManager::GetInstance().GetCurrentTime();
	}
	if (destroyed_) {
		return;
	}
	animation_->Reset();
	context_->DecreaseAmmo();
	if (interrupt_) {
		context_->TransitionTo(requested_state_);
	}
	if (context_->GetAmmo() == 0) {
		std::shared_ptr<State<Weapon>> out_of_ammo_state =
			std::make_shared<OutOfAmmoState>(weapon_name_);
		context_->TransitionTo(out_of_ammo_state);
	}
	cooldown_ = false;
}

void LoadedState::Reset() {
	animation_->Reset();
}

void LoadedState::OnContextSet() {
	animation_speed_ = context_->GetAttackSpeed();
	auto textures = TextureManager::GetInstance().GetTextureCollection(
		weapon_data_.at(weapon_name_).at("loaded"));
	animation_ = std::make_shared<TBSAnimation>(
		textures, animation_speed_ / textures.size());
}

void LoadedState::TransitionRequest(std::shared_ptr<State<Weapon>>& state) {
	if (!cooldown_) {
		context_->TransitionTo(state);
	}
	if (interrupt_) {
		return;
	}
	interrupt_ = true;
	requested_state_ = state;
}

int LoadedState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

// ########################################### OutOfAmmoState ###########################################
OutOfAmmoState::OutOfAmmoState(const std::string weapon_name)
	: weapon_name_(weapon_name),
	  last_attack_time_(0),
	  cooldown_(false),
	  interrupt_(false),
	  destroyed_(false) {}

OutOfAmmoState::~OutOfAmmoState() {
	destroyed_ = true;
}

void OutOfAmmoState::Update(const double&) {

	if (!cooldown_) {
		cooldown_ = true;
		last_attack_time_ = TimeManager::GetInstance().GetCurrentTime();
		std::thread attack_thread(&OutOfAmmoState::NoAttackAnimation, this);
		attack_thread.detach();
	}
}

WeaponStateType OutOfAmmoState::GetType() const {
	return WeaponStateType::OutOfAmmo;
}

void OutOfAmmoState::NoAttackAnimation() {
	auto attack_time = last_attack_time_;
	auto current_time = TimeManager::GetInstance().GetCurrentTime();
	while (current_time - last_attack_time_ < animation_speed_ && !interrupt_ &&
		   !destroyed_) {
		auto time_elapsed = current_time - attack_time;
		animation_->Update(time_elapsed);
		attack_time = current_time;
		current_time = TimeManager::GetInstance().GetCurrentTime();
	}
	if (destroyed_) {
		return;
	}
	if (interrupt_) {
		context_->TransitionTo(requested_state_);
	}
	animation_->Reset();
	cooldown_ = false;
}

void OutOfAmmoState::Reset() {
	animation_->Reset();
}

void OutOfAmmoState::OnContextSet() {
	animation_speed_ = context_->GetAttackSpeed();
	auto textures = TextureManager::GetInstance().GetTextureCollection(
		weapon_data_.at(weapon_name_).at("outofammo"));
	animation_ = std::make_shared<TBSAnimation>(
		textures, animation_speed_ / textures.size());
}

void OutOfAmmoState::TransitionRequest(std::shared_ptr<State<Weapon>>& state) {
	if (!cooldown_) {
		context_->TransitionTo(state);
	}
	if (interrupt_) {
		return;
	}
	interrupt_ = true;
	requested_state_ = state;
}

int OutOfAmmoState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

// ########################################### ReloadingState ###########################################
ReloadingState::ReloadingState(const std::string weapon_name)
	: weapon_name_(weapon_name),
	  last_attack_time_(0),
	  cooldown_(false),
	  interrupt_(false),
	  destroyed_(false) {}

ReloadingState::~ReloadingState() {
	destroyed_ = true;
}

void ReloadingState::Update(const double&) {
	if (!cooldown_) {
		cooldown_ = true;
		last_attack_time_ = TimeManager::GetInstance().GetCurrentTime();
		std::thread attack_thread(&ReloadingState::ReloadAnimation, this);
		attack_thread.detach();
	}
}

WeaponStateType ReloadingState::GetType() const {
	return WeaponStateType::Reloading;
}

void ReloadingState::ReloadAnimation() {
	auto attack_time = last_attack_time_;
	auto current_time = TimeManager::GetInstance().GetCurrentTime();
	while (current_time - last_attack_time_ < animation_speed_ && !interrupt_ &&
		   !destroyed_) {
		auto time_elapsed = current_time - attack_time;
		animation_->Update(time_elapsed);
		attack_time = current_time;
		current_time = TimeManager::GetInstance().GetCurrentTime();
	}
	if (destroyed_) {
		return;
	}
	context_->Charge();
	std::shared_ptr<State<Weapon>> loaded_state =
		std::make_shared<LoadedState>(weapon_name_);
	context_->TransitionTo(loaded_state);
}

void ReloadingState::Reset() {
	animation_->Reset();
}

void ReloadingState::OnContextSet() {
	animation_speed_ = context_->GetReloadSpeed();
	auto textures = TextureManager::GetInstance().GetTextureCollection(
		weapon_data_.at(weapon_name_).at("reload"));
	animation_ = std::make_shared<TBSAnimation>(
		textures, animation_speed_ / textures.size());
}

int ReloadingState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

}  // namespace wolfenstein
