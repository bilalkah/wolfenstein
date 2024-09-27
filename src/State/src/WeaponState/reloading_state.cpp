#include "State/WeaponState/reloading_state.h"
#include "Animation/time_based_single_animation.h"
#include "State/WeaponState/loaded_state.h"
#include "Strike/weapon.h"
#include "TimeManager/time_manager.h"

#include <memory>
#include <thread>

namespace wolfenstein {

ReloadingState::ReloadingState()
	: last_attack_time_(0),
	  cooldown_(false),
	  interrupt_(false),
	  destroyed_(false) {
	std::cout << "ReloadingState created" << std::endl;
}

ReloadingState::~ReloadingState() {
	destroyed_ = true;
	std::cout << "ReloadingState destroyed" << std::endl;
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
		std::make_shared<LoadedState>();
	context_->TransitionTo(loaded_state);
}

void ReloadingState::Reset() {
	animation_->Reset();
}

void ReloadingState::OnContextSet() {

	animation_ = std::make_unique<TBSAnimation>(
		context_->GetWeaponName() + "_reload", context_->GetReloadSpeed());
}

int ReloadingState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

}  // namespace wolfenstein
