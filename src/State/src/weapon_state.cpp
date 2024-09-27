/**
 * @file weapon_state.cpp
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "State/weapon_state.h"
#include "State/WeaponState/loaded_state.h"
#include "State/WeaponState/out_of_ammo_state.h"
#include "State/WeaponState/reloading_state.h"

namespace wolfenstein {

class WeaponStateImpl
{
  public:
	WeaponStateImpl() : currentState(std::make_shared<LoadedState>()) {}
	~WeaponStateImpl() = default;

	void Update(const double& delta_time) { currentState->Update(delta_time); }
	void Reset() { currentState->Reset(); }
	void SetContext(std::shared_ptr<Weapon> context) {
		currentState->SetContext(context);
	}
	void OnContextSet() { currentState->OnContextSet(); }
	void TransitionRequest(std::shared_ptr<State<Weapon>> state) {
		currentState->TransitionRequest(state);
	}
	void operator=(const WeaponStatePtr& state) { currentState = state; }
	int GetCurrentFrame() const { return currentState->GetCurrentFrame(); }
	WeaponStateType GetType() const { return currentState->GetType(); }

  private:
	WeaponStatePtr currentState;

};	// class WeaponStateImpl

WeaponState::WeaponState() : pImpl(std::make_unique<WeaponStateImpl>()) {}
WeaponState::~WeaponState() = default;

void WeaponState::Update(const double& delta_time) {
	pImpl->Update(delta_time);
}
void WeaponState::Reset() {
	pImpl->Reset();
}

void WeaponState::SetContext(std::shared_ptr<Weapon> context) {
	pImpl->SetContext(context);
}

void WeaponState::OnContextSet() {
	pImpl->OnContextSet();
}
void WeaponState::TransitionRequest(WeaponStateType state_type) {
	switch (state_type) {
		case WeaponStateType::Loaded:
			pImpl->TransitionRequest(std::make_shared<LoadedState>());
			break;
		case WeaponStateType::OutOfAmmo:
			pImpl->TransitionRequest(std::make_shared<OutOfAmmoState>());
			break;
		case WeaponStateType::Reloading:
			pImpl->TransitionRequest(std::make_shared<ReloadingState>());
			break;
		default:
			break;
	}
}

void WeaponState::operator=(const WeaponStatePtr& state) {
	pImpl->operator=(state);
}

void WeaponState::operator=(const WeaponStateType state) {
	WeaponStatePtr state_ptr;
	switch (state) {
		case WeaponStateType::Loaded:
			state_ptr = std::make_shared<LoadedState>();
			break;
		case WeaponStateType::OutOfAmmo:
			state_ptr = std::make_shared<OutOfAmmoState>();
			break;
		case WeaponStateType::Reloading:
			state_ptr = std::make_shared<ReloadingState>();
			break;
		default:
			break;
	}
	pImpl->operator=(state_ptr);
}

int WeaponState::GetCurrentFrame() const {
	return pImpl->GetCurrentFrame();
}
WeaponStateType WeaponState::GetType() const {
	return pImpl->GetType();
}

}  // namespace wolfenstein
