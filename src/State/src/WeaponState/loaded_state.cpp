#include "State/WeaponState/loaded_state.h"
#include "Animation/time_based_single_animation.h"
#include "State/WeaponState/out_of_ammo_state.h"
#include "Strike/weapon.h"
#include "TimeManager/time_manager.h"

#include <memory>
#include <thread>

namespace wolfenstein {

LoadedState::LoadedState()
	: last_attack_time_(0),
	  cooldown_(false),
	  interrupt_(false),
	  destroyed_(false) {
	std::cout << "LoadedState created" << std::endl;
}

LoadedState::~LoadedState() {
	destroyed_ = true;
	std::cout << "LoadedState destroyed" << std::endl;
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
			std::make_shared<OutOfAmmoState>();
		context_->TransitionTo(out_of_ammo_state);
	}
	cooldown_ = false;
}

void LoadedState::Reset() {
	animation_->Reset();
}

void LoadedState::OnContextSet() {
	animation_ = std::make_unique<TBSAnimation>(
		context_->GetWeaponName() + "_loaded", context_->GetAttackSpeed());
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

}  // namespace wolfenstein