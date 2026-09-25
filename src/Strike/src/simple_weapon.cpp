#include "Strike/simple_weapon.h"

namespace wolfenstein {

SimpleWeapon::SimpleWeapon(const SimpleWeaponConfig& config)
	: weapon_name_(config.weapon_name),
	  attack_damage_(config.attack_damage),
	  attack_range_(config.attack_range),
	  attack_speed_(config.attack_speed),
	  attack_rate_(config.attack_rate) {}

}  // namespace wolfenstein
