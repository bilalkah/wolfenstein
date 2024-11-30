/**
 * @file melee.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-11-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STRIKE_INCLUDE_STRIKE_SIMPLE_WEAPON_H
#define STRIKE_INCLUDE_STRIKE_SIMPLE_WEAPON_H

#include "Camera/ray.h"
#include "Strike/strike.h"
#include <memory>
#include <string>

namespace wolfenstein {

class SimpleWeapon : public IStrike,
					 public std::enable_shared_from_this<SimpleWeapon>
{
  public:
	virtual ~SimpleWeapon() = default;

	void SetCrosshairRay(Ray ray);
	std::pair<double, double> GetAttackDamage() const;
	double GetAttackRange() const;
	double GetAttackSpeed() const;
	double GetAttackRate() const;
	Ray GetCrosshair() const;
	std::string GetWeaponName() const;

  protected:
	std::pair<double, double> attack_damage;
	double attack_range;
	double attack_speed;
	double attack_rate;
	Ray crosshair_ray;
	std::string weapon_name;
};

class Melee : public SimpleWeapon
{
  public:
	Melee();
	~Melee() = default;
	void Attack() override;
};

class Rifle : public SimpleWeapon
{
  public:
	Rifle();
	~Rifle() = default;
	void Attack() override;
};

class LaserGun : public SimpleWeapon
{
  public:
	LaserGun();
	~LaserGun() = default;
	void Attack() override;
};

}  // namespace wolfenstein
#endif	// STRIKE_INCLUDE_STRIKE_SIMPLE_WEAPON_H
