/**
 * @file shooting_manager.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-11-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_MANAGER_H
#define SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_MANAGER_H

#include "Camera/single_raycaster.h"
#include "Characters/character.h"
#include "Characters/enemy.h"
#include "Characters/player.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_manager.h"
#include <memory>
#include <vector>

namespace wolfenstein {

class ShootingManager
{
  public:
	static ShootingManager& GetInstance();
	ShootingManager(const ShootingManager&) = delete;
	ShootingManager& operator=(const ShootingManager&) = delete;
	~ShootingManager() = default;

	void InitManager(std::shared_ptr<Map> map, std::shared_ptr<Player> player,
					 std::vector<std::shared_ptr<Enemy>> enemies);
	void PlayerShoot();
	void EnemyShoot();

  private:
	ShootingManager() = default;
	double CalculateDamage(const std::shared_ptr<Enemy> enemy);

	static ShootingManager* instance_;
	std::shared_ptr<Player> player_;
	std::vector<std::shared_ptr<Enemy>> enemies_;
	std::shared_ptr<Map> map_;
};

}  // namespace wolfenstein

#endif	// SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_MANAGER_H
