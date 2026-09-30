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

namespace karakale {

// How close the player's centre comes to a wall (half its width)
inline constexpr double kCollisionDistance = 0.2;

// Whether moving from pose by delta_pose would bring a body (a square
// `radius` each way from its centre: the player's kCollisionDistance, an
// enemy its own) into a wall, corners included
bool CheckWallCollision(const Map& map, const vector2d& pose,
						const vector2d& delta_pose,
						double radius = kCollisionDistance);

// Where a body of `radius` stepping from `from` towards `to` can get:
// the step's end, moved out of any of `objects` (each as solid as its
// GetCollisionRadius) it would press into, to their edge, straight away
// from their centre. The body ends touching what it met and keeps the part
// of its step along it, so it slides round round things. A step away from
// something it already touches is left alone, so nothing gets stuck where
// it stands. `self` and, if `ignore_enemies`, enemies are passed over.
vector2d ResolveObjectCollisions(std::span<IGameObject* const> objects,
								 const IGameObject* self, const vector2d& from,
								 const vector2d& to, double radius,
								 bool ignore_enemies = false);

// The same for a single round body at `centre` of `solid` radius
vector2d PushOutOf(const vector2d& centre, double solid, const vector2d& from,
				   const vector2d& to, double radius);

}  // namespace karakale

#endif	// COLLISION_MANAGER_INCLUDE_COLLISION_MANAGER_H
