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

#include "GameMap/map.h"
#include "GameObjects/game_object.h"
#include "Math/vector.h"
#include <span>

namespace wolfenstein {

// How close a character's centre may come to a wall
inline constexpr double kCollisionDistance = 0.2;

// Whether moving from pose in the direction of delta_pose would bring a
// character within kCollisionDistance of a wall
bool CheckWallCollision(const Map& map, const vector2d& pose,
						const vector2d& delta_pose);

// Whether a body of `radius` stepping from `from` to `to` would press into
// one of `objects` (each as solid as its GetCollisionRadius). A step that
// takes it further from an object it already touches is allowed, so nothing
// gets stuck where it stands. `self` and, if `ignore_enemies`, enemies are
// passed over.
bool CheckObjectCollision(std::span<IGameObject* const> objects,
						  const IGameObject* self, const vector2d& from,
						  const vector2d& to, double radius,
						  bool ignore_enemies = false);

}  // namespace wolfenstein

#endif	// COLLISION_MANAGER_INCLUDE_COLLISION_MANAGER_H
