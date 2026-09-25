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

#include "Camera/ray.h"
#include "Characters/character.h"

namespace wolfenstein {

class Player;
class Scene;
class SimpleWeapon;
class Weapon;

// What a shot fired from `eye` (facing eye.theta) hits: is_hit is set, with
// the enemy's id and distance, for the nearest living enemy the line of fire
// passes through before a wall. Part of the simulation, so it depends on the
// game state only, not on what was last drawn.
Ray Aim(const Scene& scene, const Position2D& eye);

// The player fired from `eye`: damages the enemy it hits, if any, and counts
// it in the scene if the shot kills it
void ResolvePlayerShot(Scene& scene, const Weapon& weapon,
					   const Position2D& eye);

// An enemy fired at the player
void ResolveEnemyShot(Player& player, const SimpleWeapon& weapon);

}  // namespace wolfenstein

#endif	// SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_MANAGER_H
