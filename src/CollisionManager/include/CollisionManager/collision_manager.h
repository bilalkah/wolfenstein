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

#ifndef COLLISION_MANAGER_INCLUDE_COLLISION_MANAGER_H
#define COLLISION_MANAGER_INCLUDE_COLLISION_MANAGER_H

#include "Map/map.h"
#include "Math/vector.h"

namespace wolfenstein {

// How close a character's centre may come to a wall
inline constexpr double kCollisionDistance = 0.2;

// Whether moving from pose in the direction of delta_pose would bring a
// character within kCollisionDistance of a wall
bool CheckWallCollision(const Map& map, const vector2d& pose,
						const vector2d& delta_pose);

}  // namespace wolfenstein

#endif	// COLLISION_MANAGER_INCLUDE_COLLISION_MANAGER_H
