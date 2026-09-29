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
#include "Characters/enemy.h"
#include "Math/vector.h"
#include <cstddef>
#include <optional>

namespace wolfenstein {

class Enemy;
class Player;
class Scene;
class SimpleWeapon;
class Weapon;

// How high the eye is, and so a level shot flies: half a wall
inline constexpr double kEyeHeight = 0.5;

// What a shot fired from `eye` (facing eye.theta) hits: is_hit is set, with
// the enemy's id and distance, for the nearest living enemy the line of fire
// passes through before a wall. Part of the simulation, so it depends on the
// game state only, not on what was last drawn.
//
// `pitch` is how far up the player looks (Player::GetPitch): the shot
// climbs `pitch` for every unit it flies, from the eye half a wall up, and
// passes over or under what it does not meet at that height.
Ray Aim(const Scene& scene, const Position2D& eye, double pitch = 0.0);

// Where a shot from `eye`, climbing `pitch` a unit, crosses an enemy's
// picture (a board facing the shooter, as it is drawn): across it from the
// left and down it from the top, each 0 to 1, and how far it flew. Nothing
// if it passes beside, over or under it, or through a part of the frame
// shown that is empty (between the legs, beside the head).
struct Crossing
{
	double across = 0.0;
	double down = 0.0;
	double distance = 0.0;
};
std::optional<Crossing> Cross(const Scene& scene, const Position2D& eye,
							  double pitch, const Enemy& enemy);

// A shot at the other players: the nearest it meets short of `reach`,
// where the scene's hindsight has them (Scene::SetHindsight), as the
// player in slot `shooter` fires. Its slot, how far the shot flew, where
// the player stood, and the zone struck.
struct PlayerStrike
{
	std::size_t slot = 0;
	double distance = 0.0;
	vector2d at{};
	HitZones::Zone zone = HitZones::Zone::Body;
};
std::optional<PlayerStrike> AimAtPlayers(const Scene& scene,
										 const Position2D& eye, double pitch,
										 std::size_t shooter, double reach);

// A shot that met no enemy strikes the wall `aim` hit, if within reach: a
// puff of dust there and a mark on it. The shot climbs `pitch` a unit
// flown, as Aim's does.
void MarkWall(Scene& scene, const Ray& aim, double pitch);

// What a trigger pull did: whether any of it hit an enemy, and whether any
// of it hit one in the head
struct ShotResult
{
	bool hit = false;
	bool head = false;
};

// The player fired from `eye`: damages the enemy it hits, if any, and counts
// it in the scene if the shot kills it. Fired by `shooter` (one of the
// scene's players), it strikes the other players too, each hurt once for
// all the pellets that found it.
ShotResult ResolvePlayerShot(Scene& scene, const Weapon& weapon,
							 const Position2D& eye, double pitch = 0.0,
							 const Player* shooter = nullptr);

// An enemy fired at the player; the difficulty scales the damage by
// `damage_scale`
void ResolveEnemyShot(Player& player, const SimpleWeapon& weapon,
					  double damage_scale = 1.0);

}  // namespace wolfenstein

#endif	// SHOOTING_MANAGER_INCLUDE_SHOOTING_MANAGER_SHOOTING_MANAGER_H
