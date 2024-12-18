#include "Math/vector.h"
#include "ShootingManager/shooting_helper.h"
#include "ShootingManager/shooting_manager.h"
#include "Strike/simple_weapon.h"
#include "Strike/weapon.h"
#include <algorithm>
#include <memory>

namespace wolfenstein {

ShootingManager* ShootingManager::instance_ = nullptr;

ShootingManager& ShootingManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new ShootingManager();
	}
	return *instance_;
}

ShootingManager::~ShootingManager() {
	delete instance_;
}

void ShootingManager::InitManager(const std::shared_ptr<Scene>& scene) {
	scene_ = scene;
}

void ShootingManager::PlayerShoot(const Weapon& weapon) {
	if (!weapon.GetCrosshair().is_hit) {
		return;
	}
	auto enemies_ = scene_->GetEnemies();
	auto enemy = std::find_if(
		enemies_.begin(), enemies_.end(), [&weapon](const auto& enemy) {
			return enemy->GetId() == weapon.GetCrosshair().object_id &&
				   enemy->GetHealth() > 0;
		});
	if (enemy != enemies_.end()) {
		const auto damage = CalculateDamage(weapon);
		(*enemy)->DecreaseHealth(damage);
		(*enemy)->SetAttacked(true);
		if ((*enemy)->GetHealth() <= 0) {
			scene_->DecreaseAliveEnemies();
		}
	}
}

void ShootingManager::EnemyShoot(const SimpleWeapon& weapon) {
	auto& player_ = scene_->GetPlayer();
	const auto damage =
		LinearSlope(weapon.GetAttackDamage(), weapon.GetAttackRange(),
					weapon.GetCrosshair().distance);
	player_.DecreaseHealth(damage);
}

double ShootingManager::CalculateDamage(const Weapon& weapon) {
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

}  // namespace wolfenstein
