/**
 * @file weapon_state.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STATE_INCLUDE_STATE_WEAPON_STATE_WEAPON_STATE_H_
#define STATE_INCLUDE_STATE_WEAPON_STATE_WEAPON_STATE_H_

#include "State/state.h"
#include <iostream>
#include <memory>

namespace wolfenstein {
class Weapon;
typedef std::shared_ptr<State<Weapon>> WeaponStatePtr;
template <>
struct StateType<Weapon>
{
	enum class Type { Loaded, OutOfAmmo, Reloading };
};
typedef StateType<Weapon>::Type WeaponStateType;

class WeaponStateImpl;	// Forward declaration

class WeaponState
{
  public:
	WeaponState();
	~WeaponState();
	void Update(const double&);
	void Reset();
	void SetContext(std::shared_ptr<Weapon> context);
	void OnContextSet();
	void TransitionRequest(WeaponStateType state_type);
	void operator=(const WeaponStatePtr& state);
	void operator=(const WeaponStateType state);
	int GetCurrentFrame() const;
	WeaponStateType GetType() const;

  private:
	std::unique_ptr<WeaponStateImpl> pImpl;
};
typedef std::shared_ptr<WeaponState> WeaponStateImplPtr;
}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_WEAPON_STATE_WEAPON_STATE_H_