/**
 * @file weapon.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STRIKE_INCLUDE_STRIKE_WEAPON_H
#define STRIKE_INCLUDE_STRIKE_WEAPON_H

#include "State/weapon_state.h"
#include "Strike/strike.h"
#include <cstddef>
#include <string>
namespace wolfenstein {

struct WeaponConfig
{
	std::string weapon_name;
	double attack_speed;
	double attack_damage;
	size_t ammo_capacity;
};

class Weapon : public IStrike, public std::enable_shared_from_this<Weapon>
{
  public:
	Weapon(std::string weapon_name);
	~Weapon();
	void Init();

	void Attack() override;
	int GetTextureId() const;
	void TransitionTo(std::shared_ptr<State<Weapon>> state);
	void decreaseAmmo();
	size_t getAmmo() const;
	std::string getWeaponName() const;
	void Reload();
	void reloadFinished();

  private:
	WeaponConfig weapon_config_;

	bool cooldown_;
	bool reloading_;
	std::shared_ptr<State<Weapon>> state_;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_WEAPON_H
