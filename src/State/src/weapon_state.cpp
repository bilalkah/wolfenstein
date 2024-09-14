#include "State/weapon_state.h"
#include "Strike/weapon.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <thread>

namespace wolfenstein {

LoadedState::LoadedState(const std::string weapon_name)
	: weapon_name_(weapon_name), cooldown_(false), last_attack_time_(0) {
	animation_speed_ = 0.3;
	auto textures = TextureManager::GetInstance().GetTextureCollection(
		weapon_data_[weapon_name_]["loaded"]);
	animation_ = std::make_shared<TBSAnimation>(
		textures, animation_speed_ / textures.size());
}

LoadedState::~LoadedState() {}

void LoadedState::Update(const double& delta_time) {
	if (!cooldown_) {
		cooldown_ = true;
		last_attack_time_ = TimeManager::GetInstance().GetCurrentTime();
		std::thread attack_thread(&LoadedState::AttackAnimation, this);
		attack_thread.detach();
	}
}

// BUG: Segmentation fault if game closes while the thread is running
void LoadedState::AttackAnimation() {
	auto attack_time = last_attack_time_;
	auto current_time = TimeManager::GetInstance().GetCurrentTime();
	while (current_time - last_attack_time_ < animation_speed_) {
		auto time_elapsed = current_time - attack_time;
		animation_->Update(time_elapsed);
		attack_time = current_time;
		current_time = TimeManager::GetInstance().GetCurrentTime();
	}
	animation_->Reset();
	context_->decreaseAmmo();
	cooldown_ = false;
	if (context_->getAmmo() == 0) {
		context_->TransitionTo(std::make_shared<OutOfAmmoState>(weapon_name_));
	}
}

void LoadedState::Reset() {
	animation_->Reset();
}

int LoadedState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

OutOfAmmoState::OutOfAmmoState(const std::string weapon_name)
	: weapon_name_(weapon_name), cooldown_(false), last_attack_time_(0) {
	animation_speed_ = 0.2;
	auto textures = TextureManager::GetInstance().GetTextureCollection(
		weapon_data_[weapon_name_]["outofammo"]);
	animation_ = std::make_shared<TBSAnimation>(
		textures, animation_speed_ / textures.size());
}

OutOfAmmoState::~OutOfAmmoState() {}

void OutOfAmmoState::Update(const double& delta_time) {

	if (!cooldown_) {
		cooldown_ = true;
		last_attack_time_ = TimeManager::GetInstance().GetCurrentTime();
		std::thread attack_thread(&OutOfAmmoState::NoAttackAnimation, this);
		attack_thread.detach();
	}
}

// BUG: Segmentation fault if game closes while the thread is running
void OutOfAmmoState::NoAttackAnimation() {
	auto attack_time = last_attack_time_;
	auto current_time = TimeManager::GetInstance().GetCurrentTime();
	while (current_time - last_attack_time_ < animation_speed_) {
		auto time_elapsed = current_time - attack_time;
		animation_->Update(time_elapsed);
		attack_time = current_time;
		current_time = TimeManager::GetInstance().GetCurrentTime();
	}
	animation_->Reset();
	cooldown_ = false;
}

void OutOfAmmoState::Reset() {
	animation_->Reset();
}

int OutOfAmmoState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

ReloadingState::ReloadingState(const std::string weapon_name)
	: weapon_name_(weapon_name), cooldown_(false), last_attack_time_(0) {
	animation_speed_ = 1.0;
	auto textures = TextureManager::GetInstance().GetTextureCollection(
		weapon_data_[weapon_name_]["reload"]);
	animation_ = std::make_shared<TBSAnimation>(
		textures, animation_speed_ / textures.size());
}

ReloadingState::~ReloadingState() {}

void ReloadingState::Update(const double& delta_time) {
	if (!cooldown_) {
		cooldown_ = true;
		last_attack_time_ = TimeManager::GetInstance().GetCurrentTime();
		std::thread attack_thread(&ReloadingState::ReloadAnimation, this);
		attack_thread.detach();
	}
}

// BUG: Segmentation fault if game closes while the thread is running
void ReloadingState::ReloadAnimation() {
	auto attack_time = last_attack_time_;
	auto current_time = TimeManager::GetInstance().GetCurrentTime();
	while (current_time - last_attack_time_ < animation_speed_) {
		auto time_elapsed = current_time - attack_time;
		animation_->Update(time_elapsed);
		attack_time = current_time;
		current_time = TimeManager::GetInstance().GetCurrentTime();
	}
	context_->reloadFinished();
	context_->TransitionTo(std::make_shared<LoadedState>(weapon_name_));
}

void ReloadingState::Reset() {
	animation_->Reset();
}

int ReloadingState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

}  // namespace wolfenstein
