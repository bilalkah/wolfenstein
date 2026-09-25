#include "Strike/simple_weapon.h"
#include <cstdlib>
#include <iostream>

namespace wolfenstein {

SimpleWeapon::SimpleWeapon(std::string weapon_name,
						   std::pair<double, double> attack_damage,
						   double attack_range, double attack_speed,
						   double attack_rate)
	: weapon_name_(std::move(weapon_name)),
	  attack_damage_(attack_damage),
	  attack_range_(attack_range),
	  attack_speed_(attack_speed),
	  attack_rate_(attack_rate) {}

SimpleWeapon SimpleWeapon::ForEnemy(std::string_view enemy_type) {
	if (enemy_type == "soldier") {
		return {"rifle", {15, 5}, 5.0, 0.7, 1.0};
	}
	if (enemy_type == "caco_demon") {
		return {"melee", {17, 8}, 2.0, 0.5, 1.0};
	}
	if (enemy_type == "cyber_demon") {
		return {"laser gun", {19, 10}, 7.0, 1.0, 1.0};
	}
	std::cerr << "No weapon for enemy type: " << enemy_type << '\n';
	std::exit(EXIT_FAILURE);
}

}  // namespace wolfenstein
