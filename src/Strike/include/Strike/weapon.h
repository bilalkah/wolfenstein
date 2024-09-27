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
	size_t ammo_capacity;
	double attack_damage;
	double attack_range;
	double attack_speed;
	double reload_speed;
};

class Weapon : public IStrike, public std::enable_shared_from_this<Weapon>
{
  public:
	Weapon(std::string weapon_name);
	~Weapon();
	void Init();

	void Attack() override;
	void Charge();
	void Reload();
	void TransitionTo(WeaponStatePtr& state);

	void IncreaseAmmo();
	void IncreaseAmmo(size_t amount);
	void DecreaseAmmo();
	void DecreaseAmmo(size_t amount);
	size_t GetAmmo() const;
	size_t GetAmmoCapacity() const;
	double GetAttackDamage() const;
	double GetAttackRange() const;
	double GetAttackSpeed() const;
	double GetReloadSpeed() const;
	std::string GetWeaponName() const;
	int GetTextureId() const;

  private:
	WeaponConfig weapon_properties_;
	size_t ammo_;
	WeaponStatePtr state_;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_WEAPON_H
