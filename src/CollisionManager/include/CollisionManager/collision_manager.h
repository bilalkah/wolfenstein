/**
 * @file collision_manager.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef COLLISION_MANAGER_COLLISION_MANAGER_H
#define COLLISION_MANAGER_COLLISION_MANAGER_H

#include "Map/map.h"
#include <GameObjects/game_object.h>
#include <Math/vector.h>
#include <memory>

namespace wolfenstein {

namespace {
constexpr double kCollisionDistance = 0.2;
}

class CollisionManager
{
  public:
	static CollisionManager& GetInstance();

	CollisionManager(const CollisionManager&) = delete;
	CollisionManager& operator=(const CollisionManager&) = delete;
	~CollisionManager() = default;

	void InitManager(std::shared_ptr<Map> map_ptr);
	bool CheckWallCollision(const vector2d& pose, const vector2d& delta_pose);

  private:
	CollisionManager() = default;

	static CollisionManager* instance_;
	std::shared_ptr<Map> map_ptr_;
	std::vector<std::vector<uint16_t>> map_;
};
}  // namespace wolfenstein

#endif	// COLLISION_MANAGER_COLLISION_MANAGER_H
