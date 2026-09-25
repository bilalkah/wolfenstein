#include "ShootingManager/shooting_manager.h"
#include "Characters/enemy.h"
#include "Characters/player.h"
#include "Core/scene.h"
#include "ShootingManager/shooting_helper.h"
#include "Strike/simple_weapon.h"
#include "Strike/weapon.h"
#include <algorithm>

namespace wolfenstein {

namespace {

double CalculateDamage(const Weapon& weapon) {
	if (weapon.GetCrosshair().distance > weapon.GetAttackRange()) {
		return 0;
	}
	if (weapon.GetWeaponName() == "mp5") {
		return LinearSlope(weapon.GetAttackDamage(), weapon.GetAttackRange(),
						   weapon.GetCrosshair().distance);
	}
	if (weapon.GetWeaponName() == "shotgun") {
		return ExponentialSlope(weapon.GetAttackDamage(),
								weapon.GetAttackRange(),
								weapon.GetCrosshair().distance);
	}
	return 0;
}

}  // namespace

void ResolvePlayerShot(Scene& scene, const Weapon& weapon) {
	const Ray& crosshair = weapon.GetCrosshair();
	if (!crosshair.is_hit) {
		return;
	}
	const auto enemies = scene.GetEnemies();
	const auto enemy =
		std::ranges::find_if(enemies, [&crosshair](const Enemy* candidate) {
			return candidate->GetId() == crosshair.object_id &&
				   candidate->GetHealth() > 0;
		});
	if (enemy == enemies.end()) {
		return;
	}
	(*enemy)->DecreaseHealth(CalculateDamage(weapon));
	(*enemy)->SetAttacked(true);
	if ((*enemy)->GetHealth() <= 0) {
		scene.DecreaseAliveEnemies();
	}
}

void ResolveEnemyShot(Player& player, const SimpleWeapon& weapon) {
	player.DecreaseHealth(LinearSlope(weapon.GetAttackDamage(),
									  weapon.GetAttackRange(),
									  weapon.GetCrosshair().distance));
}

}  // namespace wolfenstein
