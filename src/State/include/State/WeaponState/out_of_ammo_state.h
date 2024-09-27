/**
 * @file out_of_ammo_state.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STATE_INCLUDE_STATE_WEAPON_STATE_OUT_OF_AMMO_STATE_H_
#define STATE_INCLUDE_STATE_WEAPON_STATE_OUT_OF_AMMO_STATE_H_

#include "Animation/time_based_single_animation.h"
#include "State/weapon_state.h"
#include <memory>

namespace wolfenstein {

class OutOfAmmoState : public State<Weapon>
{
  public:
	OutOfAmmoState();
	~OutOfAmmoState();

	void Update(const double&) override;
	void Reset() override;
	void OnContextSet() override;
	void TransitionRequest(std::shared_ptr<State<Weapon>>& state) override;
	int GetCurrentFrame() const override;
	WeaponStateType GetType() const override;

  private:
	void NoAttackAnimation();
	double last_attack_time_;
	double animation_speed_;
	bool cooldown_;
	bool interrupt_;
	bool destroyed_;
	WeaponStatePtr requested_state_;
	std::unique_ptr<TBSAnimation> animation_;
};
}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_WEAPON_STATE_OUT_OF_AMMO_STATE_H_
