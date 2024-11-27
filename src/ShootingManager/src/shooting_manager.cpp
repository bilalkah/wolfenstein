#include "ShootingManager/shooting_manager.h"
#include "Math/vector.h"

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

void ShootingManager::PlayerShoot() {
	auto enemy = std::find_if(
		enemies_.begin(), enemies_.end(), [this](const auto& enemy) {
			return enemy->GetId() == player_->GetCrosshairRay()->object_id;
		});
	if (enemy != enemies_.end()) {
		(*enemy)->DecreaseHealth(CalculateDamage(*enemy));
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

void ShootingManager::EnemyShoot() {
	std::cout << "Enemy shoot" << std::endl;
	player_->DecreaseHealth(10);
	std::cout << "Player health: " << player_->GetHealth() << std::endl;
}

double ShootingManager::CalculateDamage(const std::shared_ptr<Enemy> enemy) {
	return 10;
}

}  // namespace wolfenstein
