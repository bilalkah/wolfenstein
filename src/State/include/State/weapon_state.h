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

typedef std::shared_ptr<WeaponState> WeaponStatePtr;

// ########################################### LoadedState ###########################################
class LoadedState : public WeaponState
{
  public:
	LoadedState();
	~LoadedState();

	void Update(const double&) override;
	void OnContextSet() override;
	WeaponStateType GetType() const override;

	void PullTrigger() override;

  private:
	bool trigger_pulled_;
	double trigger_pull_time_;
	double fire_rate_;
};

// ########################################### OutOfAmmoState ###########################################
class OutOfAmmoState : public WeaponState
{
  public:
	OutOfAmmoState();
	~OutOfAmmoState();

	void Update(const double&) override;
	void OnContextSet() override;
	WeaponStateType GetType() const override;

	void PullTrigger() override;

  private:
	bool trigger_pulled_;
	double trigger_pull_time_;
	double fire_rate_;
};

// ########################################### ReloadingState ###########################################
class ReloadingState : public WeaponState
{
  public:
	ReloadingState();
	~ReloadingState();

	void Update(const double&) override;
	void OnContextSet() override;
	WeaponStateType GetType() const override;

  private:
	double reload_time_;
	double reload_speed_;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_WEAPON_STATE_H_
