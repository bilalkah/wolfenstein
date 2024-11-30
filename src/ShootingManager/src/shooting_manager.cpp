#include "ShootingManager/shooting_manager.h"
#include "Math/vector.h"
#include "ShootingManager/shooting_helper.h"
#include "Strike/simple_weapon.h"
#include "Strike/weapon.h"
#include <memory>

namespace wolfenstein {

ShootingManager* ShootingManager::instance_ = nullptr;

ShootingManager& ShootingManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new ShootingManager();
	}
	return *instance_;
}

void ShootingManager::InitManager(std::shared_ptr<Map> map,
								  std::shared_ptr<Player> player,
								  std::vector<std::shared_ptr<Enemy>> enemies) {
	map_ = map;
	player_ = player;
	enemies_ = enemies;
}

void ShootingManager::PlayerShoot(const std::shared_ptr<Weapon> weapon) {
	auto enemy = std::find_if(
		enemies_.begin(), enemies_.end(), [this](const auto& enemy) {
			return enemy->GetId() == player_->GetCrosshairRay()->object_id;
		});
	if (enemy != enemies_.end()) {
		const auto damage = CalculateDamage(weapon);
		if (damage == 0) {
			return;
		}
		(*enemy)->DecreaseHealth(damage);
		(*enemy)->SetAttacked(true);
		std::cout << "Enemy Name: " << (*enemy)->GetBotName()
				  << " Enemy health: " << (*enemy)->GetHealth() << std::endl;
		if ((*enemy)->GetHealth() <= 0) {
			// remove and erase
			std::cout << "Enemy is dead" << std::endl;
			enemies_.erase(enemy);
		}
	}
}

void ShootingManager::EnemyShoot(const std::shared_ptr<SimpleWeapon> weapon) {
	std::cout << "Enemy shoot with " << weapon->GetWeaponName() << std::endl;
	const auto damage =
		LinearSlope(weapon->GetAttackDamage(), weapon->GetAttackRange(),
					weapon->GetCrosshair().distance);
	player_->DecreaseHealth(damage);
	std::cout << "Damage dealt: " << damage
			  << " Player health: " << player_->GetHealth() << std::endl;
}

double ShootingManager::CalculateDamage(const std::shared_ptr<Weapon> weapon) {
	if (weapon->GetCrosshair()->distance > weapon->GetAttackRange()) {
		return 0;
	}
	if (weapon->GetWeaponName() == "mp5") {
		return LinearSlope(weapon->GetAttackDamage(), weapon->GetAttackRange(),
						   weapon->GetCrosshair()->distance);
	}
	if (weapon->GetWeaponName() == "shotgun") {
		return ExponentialSlope(weapon->GetAttackDamage(),
								weapon->GetAttackRange(),
								weapon->GetCrosshair()->distance);
	}
	return 0;
}

}  // namespace wolfenstein
