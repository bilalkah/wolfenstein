/**
 * @file weapon_state.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-06
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STATE_INCLUDE_STATE_WEAPON_STATE_H_
#define STATE_INCLUDE_STATE_WEAPON_STATE_H_

#include "Animation/looped_animation.h"
#include "State/state.h"
#include <memory>

namespace wolfenstein {

class Weapon;

template <>
struct StateType<Weapon>
{
	enum class Type { Loaded, OutOfAmmo, Reloading };
};
typedef StateType<Weapon>::Type WeaponStateType;

class WeaponState : public State<Weapon>
{
  public:
	virtual ~WeaponState() = default;
	virtual void PullTrigger() {};
	void Reset() override;
	int GetCurrentFrame() const override;

  protected:
	std::unique_ptr<LoopedAnimation> animation_;
};

using WeaponStatePtr = std::unique_ptr<WeaponState>;

// ########################################### LoadedState ###########################################
class LoadedState : public WeaponState
{
  public:
	void Update(const double&) override;
	void OnContextSet() override;
	WeaponStateType GetType() const override;

	void PullTrigger() override;

  private:
	bool trigger_pulled_{false};
	double trigger_pull_time_{0.0};
	double fire_rate_{0.0};
};

// ########################################### OutOfAmmoState ###########################################
class OutOfAmmoState : public WeaponState
{
  public:
	void Update(const double&) override;
	void OnContextSet() override;
	WeaponStateType GetType() const override;

	void PullTrigger() override;

  private:
	bool trigger_pulled_{false};
	double trigger_pull_time_{0.0};
	double fire_rate_{0.0};
};

// ########################################### ReloadingState ###########################################
class ReloadingState : public WeaponState
{
  public:
	void Update(const double&) override;
	void OnContextSet() override;
	WeaponStateType GetType() const override;

  private:
	double reload_time_{0.0};
	double reload_speed_{0.0};
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_WEAPON_STATE_H_
