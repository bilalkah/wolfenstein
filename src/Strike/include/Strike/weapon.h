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
#include <memory>
#include <string>
namespace wolfenstein {

class Ray;
struct WeaponConfig
{
	WeaponConfig(std::string weapon_name, size_t ammo_capacity,
				 double attack_damage_max, double attack_damage_min,
				 double attack_range, double attack_speed, double reload_speed)
		: weapon_name(weapon_name),
		  ammo_capacity(ammo_capacity),
		  attack_damage(attack_damage_max, attack_damage_min),
		  attack_range(attack_range),
		  attack_speed(attack_speed),
		  reload_speed(reload_speed) {}
	std::string weapon_name;
	size_t ammo_capacity;
	std::pair<double, double> attack_damage;
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
	void Update(double delta_time);
	void Charge();
	void Reload();
	void TransitionTo(WeaponStatePtr& state);

	void SetAmmo(size_t ammo);
	void IncreaseAmmo();
	void IncreaseAmmo(size_t amount);
	void DecreaseAmmo();
	void DecreaseAmmo(size_t amount);
	void SetCrossHair(std::shared_ptr<Ray> crosshair);

	size_t GetAmmo() const;
	size_t GetAmmoCapacity() const;
	std::pair<double, double> GetAttackDamage() const;
	double GetAttackRange() const;
	double GetAttackSpeed() const;
	double GetReloadSpeed() const;
	std::string GetWeaponName() const;
	int GetTextureId() const;
	std::shared_ptr<Ray> GetCrosshair() const;

  private:
	WeaponConfig weapon_properties_;
	size_t ammo_;
	WeaponStatePtr state_;
	bool cooldown_;
	double attack_time_;
	std::shared_ptr<Ray> crosshair_;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_WEAPON_H
