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
#include "Core/scene.h"
#include "Math/vector.h"
#include "NavigationManager/navigation_manager.h"
#include "Strike/simple_weapon.h"
#include "Strike/strike.h"
#include <memory>
#include <vector>

namespace wolfenstein {

class ShootingManager
{
  public:
	static ShootingManager& GetInstance();
	ShootingManager(const ShootingManager&) = delete;
	ShootingManager& operator=(const ShootingManager&) = delete;
	~ShootingManager();

	void InitManager(std::shared_ptr<Scene> scene);
	void PlayerShoot(const Weapon& weapon);
	void EnemyShoot(const SimpleWeapon& weapon);

  private:
	ShootingManager() = default;
	double CalculateDamage(const Weapon& weapon);

	static ShootingManager* instance_;
	std::shared_ptr<Scene> scene_;
};

}  // namespace wolfenstein

#endif	// SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_MANAGER_H
