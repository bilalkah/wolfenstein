/**
 * @file reloading_state.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STATE_INCLUDE_STATE_WEAPON_STATE_RELOADING_STATE_H_
#define STATE_INCLUDE_STATE_WEAPON_STATE_RELOADING_STATE_H_

#include "Animation/time_based_single_animation.h"
#include "State/weapon_state.h"
#include <memory>

namespace wolfenstein {

class ReloadingState : public State<Weapon>
{
  public:
	ReloadingState();
	~ReloadingState();

	void Update(const double&) override;
	void Reset() override;
	void OnContextSet() override;
	int GetCurrentFrame() const override;
	WeaponStateType GetType() const override;

  private:
	void ReloadAnimation();
	double last_attack_time_;
	double animation_speed_;
	bool cooldown_;
	bool interrupt_;
	bool destroyed_;
	std::unique_ptr<TBSAnimation> animation_;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_WEAPON_STATE_RELOADING_STATE_H_
