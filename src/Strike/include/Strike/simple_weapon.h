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
#include <string>
#include <string_view>
#include <utility>

namespace wolfenstein {

// An enemy weapon as config.json describes it
struct SimpleWeaponConfig
{
	std::string weapon_name;
	std::pair<double, double> attack_damage;  // at point blank, at range
	double attack_range{};
	double attack_speed{};
	double attack_rate{};
};

// An enemy's weapon: its stats and what its crosshair currently points at.
// A plain value its enemy holds; firing is ResolveEnemyShot's job.
class SimpleWeapon
{
  public:
	explicit SimpleWeapon(const SimpleWeaponConfig& config);

	void SetCrosshairRay(const Ray& ray) { crosshair_ray_ = ray; }
	std::pair<double, double> GetAttackDamage() const { return attack_damage_; }
	double GetAttackRange() const { return attack_range_; }
	double GetAttackSpeed() const { return attack_speed_; }
	double GetAttackRate() const { return attack_rate_; }
	const Ray& GetCrosshair() const { return crosshair_ray_; }
	const std::string& GetWeaponName() const { return weapon_name_; }

  private:
	std::string weapon_name_;
	std::pair<double, double> attack_damage_;  // at point blank, at range
	double attack_range_{};
	double attack_speed_{};
	double attack_rate_{};
	Ray crosshair_ray_;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_SIMPLE_WEAPON_H
