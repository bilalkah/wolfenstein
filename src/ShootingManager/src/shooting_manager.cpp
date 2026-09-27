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
#include <array>
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

// A shot that met no enemy strikes the wall it hit (if within reach): a
// puff of dust there and a mark on it. Doors and secret walls move, so keep
// no marks.
void MarkWall(Scene& scene, const Ray& aim) {
	if (aim.wall_id == 0) {
		return;
	}
	const vector2d toward_eye = aim.direction * -1.0;
	scene.ShowImpact(Scene::Impact::Dust, aim.hit_point + toward_eye * 0.05);
	if (Map::IsDoorCell(static_cast<std::uint16_t>(aim.wall_id))) {
		return;
	}
	const auto [x, y] = HitCell(aim);
	if (scene.GetMap().FindPushWall(x, y) != nullptr) {
		return;
	}
	const double along =
		aim.is_hit_vertical ? aim.hit_point.y : aim.hit_point.x;
	// Shots fly half a wall up; a little scatter keeps marks from stacking
	static constexpr std::array<float, 7> kScatter{0.0F,   0.03F, -0.04F, 0.05F,
												   -0.02F, 0.04F, -0.05F};
	constexpr double kSpread = 977.0;  // any large step scatters the index
	const auto shot =
		static_cast<std::size_t>(std::fabs(along) * kSpread) % kScatter.size();
	scene.AddWallMark({.x = x,
					   .y = y,
					   .face = HitFace(aim),
					   .across = static_cast<float>(along - std::floor(along)),
					   .down = 0.5F + kScatter[shot]});
}

void ResolveOneShot(Scene& scene, const Weapon& weapon, const Position2D& eye) {
	const Ray aim = Aim(scene, eye);
	if (!aim.is_hit) {
		if (!weapon.IsMelee()) {
			MarkWall(scene, aim);
		}
		return;
	}
	// A blade reaches only the enemy in front of it
	if (weapon.IsMelee() && aim.distance > weapon.GetAttackRange()) {
		return;
	}
	const auto enemies = scene.GetEnemies();
	const auto enemy =
		std::ranges::find_if(enemies, [&aim](const Enemy* candidate) {
			return candidate->GetId() == aim.object_id;
		});
	// Blood, in front of the enemy as the shooter sees it
	const vector2d back = eye.pose - aim.hit_point;
	scene.ShowImpact(Scene::Impact::Blood,
					 aim.hit_point + back * ((*enemy)->GetWidth() / 2 + 0.05) /
										 std::max(aim.distance, 0.01));
	(*enemy)->DecreaseHealth(CalculateDamage(weapon, aim.distance));
	(*enemy)->SetAttacked(true);
	// Aim only offers enemies with health left, so this is the killing shot,
	// counted once
	if ((*enemy)->GetHealth() <= 0) {
		scene.DecreaseAliveEnemies();
	}
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
	// Each pellet flies on its own line, fanned evenly across the spread
	const std::size_t pellets = std::max<std::size_t>(weapon.GetPellets(), 1);
	for (std::size_t pellet = 0; pellet < pellets; ++pellet) {
		const double offset =
			pellets == 1
				? 0.0
				: weapon.GetSpread() * (static_cast<double>(pellet) /
											static_cast<double>(pellets - 1) -
										0.5);
		ResolveOneShot(scene, weapon,
					   Position2D(eye.pose, SumRadian(eye.theta, offset)));
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
