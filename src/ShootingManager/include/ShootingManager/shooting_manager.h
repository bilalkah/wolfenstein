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

namespace wolfenstein {

class Player;
class Scene;
class SimpleWeapon;
class Weapon;

// The player fired: damages the living enemy under the weapon's crosshair,
// if there is one, and counts it in the scene if the shot kills it
void ResolvePlayerShot(Scene& scene, const Weapon& weapon);

// An enemy fired at the player
void ResolveEnemyShot(Player& player, const SimpleWeapon& weapon);

}  // namespace wolfenstein

#endif	// SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_MANAGER_H
