#include "ShootingManager/shooting_manager.h"
#include "Camera/raycaster.h"
#include "Characters/enemy.h"
#include "Characters/player.h"
#include "Core/scene.h"
#include "Math/vector.h"
#include "ShootingManager/shooting_helper.h"
#include "Strike/simple_weapon.h"
#include "Strike/weapon.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace wolfenstein {

namespace {

double CalculateDamage(const Weapon& weapon, double distance) {
	if (distance > weapon.GetAttackRange()) {
		return 0;
	}
	switch (weapon.GetFalloff()) {
		case DamageFalloff::Linear:
			return LinearSlope(weapon.GetAttackDamage(),
							   weapon.GetAttackRange(), distance);
		case DamageFalloff::Exponential:
			return ExponentialSlope(weapon.GetAttackDamage(),
									weapon.GetAttackRange(), distance);
	}
	std::unreachable();
}

}  // namespace

Ray Aim(const Scene& scene, const Position2D& eye) {
	// How far a shot reaches: as far as the player can see (the view
	// distance), beyond which the camera never offered an enemy as a target
	constexpr double kReach = 15.0;
	Ray aim = CastRay(scene.GetMap(), eye, eye.theta, kReach);
	const double wall_distance = std::min(aim.distance, kReach);
	aim.is_hit = false;	 // from here: whether an enemy is hit

	const vector2d facing{std::cos(eye.theta), std::sin(eye.theta)};
	// Signed angle between the line of fire and the direction to `point`
	const auto angle_to = [&](const vector2d& point) {
		const double angle =
			std::atan2(point.y - eye.pose.y, point.x - eye.pose.x);
		return CalculateAngleBetweenTwoVectorsSigned(
			{std::cos(angle), std::sin(angle)}, facing);
	};

	double nearest = wall_distance;
	for (const Enemy* enemy : scene.GetEnemies()) {
		// A falling enemy (shot dead, its death still playing) no longer
		// stops a shot
		if (!enemy->IsAlive() || enemy->GetHealth() <= 0) {
			continue;
		}
		const vector2d pose = enemy->GetPose();
		const double distance = pose.Distance(eye.pose);
		if (distance >= nearest) {
			continue;
		}
		// The enemy's left and right edges, as seen from the eye: the line
		// of fire hits it if it passes between them
		const double half_width = enemy->GetWidth() / 2;
		const double centre =
			std::atan2(pose.y - eye.pose.y, pose.x - eye.pose.x);
		const double left_edge = SubRadian(centre, ToRadians(90.0));
		const double right_edge = SumRadian(centre, ToRadians(90.0));
		const double left =
			angle_to(pose + vector2d{half_width * std::cos(left_edge),
									 half_width * std::sin(left_edge)});
		const double right =
			angle_to(pose + vector2d{half_width * std::cos(right_edge),
									 half_width * std::sin(right_edge)});
		if (left <= 0 && right >= 0) {
			nearest = distance;
			aim.is_hit = true;
			aim.distance = distance;
			aim.object_id = enemy->GetId();
			aim.hit_point = pose;
		}
	}
	return aim;
}

void ResolvePlayerShot(Scene& scene, const Weapon& weapon,
					   const Position2D& eye) {
	const Ray aim = Aim(scene, eye);
	if (!aim.is_hit) {
		return;
	}
	const auto enemies = scene.GetEnemies();
	const auto enemy =
		std::ranges::find_if(enemies, [&aim](const Enemy* candidate) {
			return candidate->GetId() == aim.object_id;
		});
	(*enemy)->DecreaseHealth(CalculateDamage(weapon, aim.distance));
	(*enemy)->SetAttacked(true);
	// Aim only offers enemies with health left, so this is the killing shot,
	// counted once
	if ((*enemy)->GetHealth() <= 0) {
		scene.DecreaseAliveEnemies();
	}
}

void ResolveEnemyShot(Player& player, const SimpleWeapon& weapon,
					  double damage_scale) {
	player.DecreaseHealth(damage_scale *
						  LinearSlope(weapon.GetAttackDamage(),
									  weapon.GetAttackRange(),
									  weapon.GetCrosshair().distance));
}

}  // namespace wolfenstein
