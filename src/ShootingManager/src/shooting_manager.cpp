#include "ShootingManager/shooting_manager.h"
#include "Camera/raycaster.h"
#include "Characters/enemy.h"
#include "Characters/player.h"
#include "Core/scene.h"
#include "Math/vector.h"
#include "ShootingManager/shooting_helper.h"
#include "Strike/simple_weapon.h"
#include "Strike/weapon.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
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

// Doors and secret walls move, so keep no marks
void MarkWall(Scene& scene, const Ray& aim, double pitch) {
	// Where on the wall's height the shot meets it: past the top or below
	// the bottom, it struck the ceiling or the floor first, and shows nothing
	const double height = kEyeHeight + pitch * aim.distance;
	if (aim.wall_id == 0 || height < 0.0 || height > 1.0) {
		return;
	}
	const vector2d toward_eye = aim.direction * -1.0;
	scene.ShowImpact(Scene::Impact::Dust, aim.hit_point + toward_eye * 0.05,
					 height);
	if (Map::IsDoorCell(static_cast<std::uint16_t>(aim.wall_id))) {
		return;
	}
	const auto [x, y] = HitCell(aim);
	if (scene.GetMap().FindPushWall(x, y) != nullptr) {
		return;
	}
	const double along =
		aim.is_hit_vertical ? aim.hit_point.y : aim.hit_point.x;
	// A little scatter keeps marks from stacking
	static constexpr std::array<float, 7> kScatter{0.0F,   0.03F, -0.04F, 0.05F,
												   -0.02F, 0.04F, -0.05F};
	constexpr double kSpread = 977.0;  // any large step scatters the index
	const auto shot =
		static_cast<std::size_t>(std::fabs(along) * kSpread) % kScatter.size();
	scene.AddWallMark(
		{.x = x,
		 .y = y,
		 .face = HitFace(aim),
		 .across = static_cast<float>(along - std::floor(along)),
		 .down = std::clamp(static_cast<float>(1.0 - height) + kScatter[shot],
							0.02F, 0.98F)});
}

namespace {

// How far a shot reaches: as far as the player can see (the view distance),
// beyond which the camera never offered an enemy as a target
constexpr double kReach = 15.0;

// Where a shot from `eye`, climbing `pitch` a unit, crosses a board standing
// at `pose` facing it, `width` across and `height` tall: across it from the
// left and down it from the top, and how far it flew; nothing if it passes
// beside, over or under it
std::optional<Crossing> CrossBoard(const Position2D& eye, double pitch,
								   const vector2d& pose, double width,
								   double height) {
	const vector2d to = pose - eye.pose;
	const double distance = to.Magnitude();
	constexpr double kTouching = 1e-9;
	if (distance < kTouching) {
		return std::nullopt;
	}
	const vector2d towards = to / distance;
	const vector2d facing{std::cos(eye.theta), std::sin(eye.theta)};
	const double approach = facing.Dot(towards);
	if (approach <= 0.0) {
		return std::nullopt;  // behind the shooter
	}
	// How far the shot flies to the board, and where on it it passes
	const double along = distance / approach;
	const vector2d off_centre = facing * along - to;
	const vector2d right{-towards.y, towards.x};  // the viewer's right
	const double across = 0.5 + off_centre.Dot(right) / width;
	const double down = 1.0 - (kEyeHeight + pitch * along) / height;
	if (across < 0.0 || across >= 1.0 || down < 0.0 || down >= 1.0) {
		return std::nullopt;
	}
	return Crossing{.across = across, .down = down, .distance = along};
}

// What a trigger pull's pellets did to the other players: a player struck
// is hurt once they have all flown, so the damage adds up here
struct Dealt
{
	std::array<double, Scene::kMaxPlayers> damage{};
	std::array<bool, Scene::kMaxPlayers> head{};
};

// One pellet: an enemy it strikes is hurt at once, what it does to a
// player adds up in `dealt`; the result is the enemy's
ShotResult ResolveOneShot(Scene& scene, const Weapon& weapon,
						  const Position2D& eye, double pitch,
						  const Player* shooter, Dealt& dealt) {
	const Ray aim = Aim(scene, eye, pitch);
	// Another player nearer than the enemy or wall the shot meets
	const double reach = std::min(
		aim.distance, weapon.IsMelee() ? weapon.GetAttackRange() : kReach);
	if (const auto strike =
			shooter != nullptr
				? AimAtPlayers(scene, eye, pitch, shooter->Slot(), reach)
				: std::nullopt) {
		const HitZones& zones = scene.GetPlayerTarget().zones;
		const Player& struck = *scene.GetPlayers()[strike->slot];
		const bool head = strike->zone == HitZones::Zone::Head;
		constexpr double kHeadBurst = 1.6;
		const vector2d back = eye.pose - strike->at;
		scene.ShowImpact(Scene::Impact::Blood,
						 strike->at + back * (struck.GetWidth() / 2 + 0.05) /
										  std::max(strike->distance, 0.01),
						 kEyeHeight + pitch * strike->distance,
						 head ? kHeadBurst : 1.0);
		dealt.damage[strike->slot] += zones.Scale(strike->zone) *
									  CalculateDamage(weapon, strike->distance);
		dealt.head[strike->slot] = dealt.head[strike->slot] || head;
		return {};
	}
	if (!aim.is_hit) {
		if (!weapon.IsMelee()) {
			MarkWall(scene, aim, pitch);
		}
		return {};
	}
	// A blade reaches only the enemy in front of it
	if (weapon.IsMelee() && aim.distance > weapon.GetAttackRange()) {
		return {};
	}
	const auto enemies = scene.GetEnemies();
	const auto enemy =
		std::ranges::find_if(enemies, [&aim](const Enemy* candidate) {
			return candidate->GetId() == aim.object_id;
		});
	// Where on the figure it struck: its head, body or legs, as shares of
	// the frame's visible part
	HitZones::Zone zone = HitZones::Zone::Body;
	const HitZones& zones = (*enemy)->GetHitZones();
	if (const auto crossing = Cross(scene, eye, pitch, **enemy)) {
		const auto [top, bottom] =
			scene.Textures().SolidRows((*enemy)->SeenFrom(eye.pose).texture_id);
		zone = zones.ZoneAt(std::clamp(
			(crossing->down - top) / std::max(bottom - top, 1e-6), 0.0, 1.0));
	}
	// Blood, in front of the enemy as the shooter sees it, where it was hit:
	// a bigger burst from the head
	constexpr double kHeadBurst = 1.6;
	const vector2d back = eye.pose - aim.hit_point;
	scene.ShowImpact(Scene::Impact::Blood,
					 aim.hit_point + back * ((*enemy)->GetRadius() + 0.05) /
										 std::max(aim.distance, 0.01),
					 kEyeHeight + pitch * aim.distance,
					 zone == HitZones::Zone::Head ? kHeadBurst : 1.0);
	scene.Wound(**enemy,
				zones.Scale(zone) * CalculateDamage(weapon, aim.distance));
	return {.hit = true, .head = zone == HitZones::Zone::Head};
}

}  // namespace

std::optional<Crossing> Cross(const Scene& scene, const Position2D& eye,
							  double pitch, const Enemy& enemy) {
	auto crossing = CrossBoard(eye, pitch, enemy.GetPose(), enemy.GetWidth(),
							   enemy.GetHeight());
	if (crossing &&
		!scene.Textures().IsSolidAt(enemy.SeenFrom(eye.pose).texture_id,
									crossing->across, crossing->down)) {
		return std::nullopt;
	}
	return crossing;
}

std::optional<PlayerStrike> AimAtPlayers(const Scene& scene,
										 const Position2D& eye, double pitch,
										 std::size_t shooter, double reach) {
	const PlayerTarget& target = scene.GetPlayerTarget();
	const Hindsight* hindsight = scene.GetHindsight();
	const auto players = scene.GetPlayers();
	std::optional<PlayerStrike> nearest;
	for (std::size_t slot = 0; slot < players.size(); ++slot) {
		const Player* player = players[slot];
		if (slot == shooter || player == nullptr || !player->IsAlive()) {
			continue;
		}
		const std::optional<vector2d> seen =
			hindsight != nullptr ? hindsight->Seen(shooter, slot)
								 : std::optional(player->GetPose());
		if (!seen) {
			continue;
		}
		// Its board, from the floor to the top of its picture's solid part
		const auto crossing =
			CrossBoard(eye, pitch, *seen,
					   player->GetWidth() * target.width_scale, target.top);
		const double height = target.top - target.bottom;
		if (!crossing || crossing->distance >= reach ||
			(nearest && crossing->distance >= nearest->distance) ||
			crossing->down * target.top > height) {
			continue;  // missed it, or struck something nearer first
		}
		nearest = PlayerStrike{
			.slot = slot,
			.distance = crossing->distance,
			.at = *seen,
			.zone = target.zones.ZoneAt(crossing->down * target.top /
										std::max(height, 1e-6))};
	}
	return nearest;
}

Ray Aim(const Scene& scene, const Position2D& eye, double pitch) {
	Ray aim = CastRay(scene.GetMap(), eye, eye.theta, kReach);
	const double wall_distance = std::min(aim.distance, kReach);
	aim.is_hit = false;	 // from here: whether an enemy is hit

	double nearest = wall_distance;
	for (const Enemy* enemy : scene.GetEnemies()) {
		// A falling enemy (shot dead, its death still playing) no longer
		// stops a shot
		if (!enemy->IsAlive() || enemy->GetHealth() <= 0) {
			continue;
		}
		const double distance = enemy->GetPose().Distance(eye.pose);
		if (distance < nearest && Cross(scene, eye, pitch, *enemy)) {
			nearest = distance;
			aim.is_hit = true;
			aim.distance = distance;
			aim.object_id = enemy->GetId();
			aim.hit_point = enemy->GetPose();
		}
	}
	return aim;
}

ShotResult ResolvePlayerShot(Scene& scene, const Weapon& weapon,
							 const Position2D& eye, double pitch,
							 const Player* shooter) {
	// Each pellet flies on its own line, fanned evenly across the spread
	const std::size_t pellets = std::max<std::size_t>(weapon.GetPellets(), 1);
	ShotResult result;
	Dealt dealt{};
	for (std::size_t pellet = 0; pellet < pellets; ++pellet) {
		const double offset =
			pellets == 1
				? 0.0
				: weapon.GetSpread() * (static_cast<double>(pellet) /
											static_cast<double>(pellets - 1) -
										0.5);
		const ShotResult one = ResolveOneShot(
			scene, weapon, Position2D(eye.pose, SumRadian(eye.theta, offset)),
			pitch, shooter, dealt);
		result.hit = result.hit || one.hit;
		result.head = result.head || one.head;
	}
	if (shooter == nullptr) {
		return result;
	}
	// Each player struck, hurt once by all that found it; one past hurting
	// (protected) shows no hit
	for (std::size_t slot = 0; slot < Scene::kMaxPlayers; ++slot) {
		Player* player = scene.GetPlayers()[slot];
		if (player != nullptr &&
			scene.HurtPlayer(*player, dealt.damage[slot], shooter->Slot(),
							 shooter->HeldWeapon())) {
			result.hit = true;
			result.head = result.head || dealt.head[slot];
		}
	}
	return result;
}

void ResolveEnemyShot(Player& player, const SimpleWeapon& weapon,
					  double damage_scale) {
	player.DecreaseHealth(damage_scale *
						  LinearSlope(weapon.GetAttackDamage(),
									  weapon.GetAttackRange(),
									  weapon.GetCrosshair().distance));
}

}  // namespace wolfenstein
