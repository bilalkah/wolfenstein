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
#include <cstddef>
#include <cstdint>
#include <memory>

namespace wolfenstein {

class Weapon;

template <>
struct StateType<Weapon>
{
	enum class Type : std::uint8_t { Loaded, OutOfAmmo, Reloading, Raising };
};
using WeaponStateType = StateType<Weapon>::Type;

class WeaponState : public State<Weapon>
{
  public:
	~WeaponState() override = default;

  protected:
	// Copies and moves only through derived classes: copying through the
	// base would slice off the derived part
	WeaponState() = default;
	WeaponState(const WeaponState&) = default;
	WeaponState& operator=(const WeaponState&) = default;
	WeaponState(WeaponState&&) = default;
	WeaponState& operator=(WeaponState&&) = default;

  public:
	// True if the pull fired a shot
	virtual bool PullTrigger() { return false; }
	void Reset() override;
	int GetCurrentFrame() const override;

  protected:
	LoopedAnimation animation_;
};

// ########################################### LoadedState ###########################################
class LoadedState : public WeaponState
{
  public:
	void Update(const double&) override;
	void OnContextSet() override;
	void OnEnter() override;
	WeaponStateType GetType() const override;

	bool PullTrigger() override;

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
	void OnEnter() override;
	WeaponStateType GetType() const override;

	bool PullTrigger() override;

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
	void OnEnter() override;
	WeaponStateType GetType() const override;

  private:
	double reload_time_{0.0};
	double reload_speed_{0.0};
};

// ########################################### RaisingState ###########################################
// Taken in hand: the gun comes up (its raise clip, or else the end of its
// reload), then it is ready, or empty; meanwhile it neither fires nor
// reloads
class RaisingState : public WeaponState
{
  public:
	// The share of the reload clip, from its end, that shows it
	static constexpr std::size_t kShareOfReload = 3;  // a third

	void Update(const double&) override;
	void OnContextSet() override;
	void OnEnter() override;
	WeaponStateType GetType() const override;

  private:
	double time_{0.0};
	double seconds_{0.0};  // its weapon's raise_seconds
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_WEAPON_STATE_H_
