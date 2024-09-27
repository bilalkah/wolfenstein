#include "State/WeaponState/out_of_ammo_state.h"
#include "Animation/time_based_single_animation.h"
#include "Strike/weapon.h"
#include "TimeManager/time_manager.h"

#include <memory>
#include <thread>

namespace wolfenstein {

OutOfAmmoState::OutOfAmmoState()
	: last_attack_time_(0),
	  cooldown_(false),
	  interrupt_(false),
	  destroyed_(false) {
	std::cout << "OutOfAmmoState created" << std::endl;
}

OutOfAmmoState::~OutOfAmmoState() {
	destroyed_ = true;
	std::cout << "OutOfAmmoState destroyed" << std::endl;
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
	animation_ = std::make_unique<TBSAnimation>(
		context_->GetWeaponName() + "_outofammo", context_->GetAttackSpeed());
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

}  // namespace wolfenstein
