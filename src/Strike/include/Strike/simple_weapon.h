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

// An enemy's weapon: its stats (borrowed from the config) and what its
// crosshair currently points at. Its enemy holds it by value; firing is
// ResolveEnemyShot's job.
class SimpleWeapon
{
  public:
	explicit SimpleWeapon(const SimpleWeaponConfig& config);

	void SetCrosshairRay(const Ray& ray) { crosshair_ray_ = ray; }
	std::pair<double, double> GetAttackDamage() const {
		return config_->attack_damage;
	}
	double GetAttackRange() const { return config_->attack_range; }
	double GetAttackSpeed() const { return config_->attack_speed; }
	double GetAttackRate() const { return config_->attack_rate; }
	const Ray& GetCrosshair() const { return crosshair_ray_; }
	const std::string& GetWeaponName() const { return config_->weapon_name; }

  private:
	// Borrowed from the game config, which outlives every enemy
	const SimpleWeaponConfig* config_;
	Ray crosshair_ray_;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_SIMPLE_WEAPON_H
