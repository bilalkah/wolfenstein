// Keeps the numbers in config.json fair: how many hits each weapon takes to
// bring a soldier down, how many a soldier takes to bring the player down,
// and that every level carries the ammunition its enemies need, at every
// difficulty

#include "Core/level_data.h"
#include "ShootingManager/shooting_helper.h"
#include "test_services.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <set>
#include <string>

namespace wolfenstein {
namespace {

// A fight's usual distance, in map units: across a room
constexpr double kRoom = 3.0;

// What one trigger pull does to an enemy `distance` away, every pellet on it
double Damage(const WeaponConfig& weapon, double distance) {
	const double one =
		weapon.falloff == DamageFalloff::Linear
			? LinearSlope(weapon.attack_damage, weapon.attack_range, distance)
			: ExponentialSlope(weapon.attack_damage, weapon.attack_range,
							   distance);
	return one * static_cast<double>(weapon.pellets);
}

int HitsToKill(double health, double damage) {
	return static_cast<int>(std::ceil(health / damage));
}

double SoldierHealth() {
	return testing::GameData().enemies.find("soldier")->second.health;
}

TEST(Balance, ThePistolTakesAFewShotsAcrossARoom) {
	const int shots =
		HitsToKill(SoldierHealth(), Damage(testing::Weapon("pistol"), kRoom));
	EXPECT_GE(shots, 3);
	EXPECT_LE(shots, 6);
}

TEST(Balance, TheShotgunDropsASoldierPointBlank) {
	EXPECT_EQ(
		HitsToKill(SoldierHealth(), Damage(testing::Weapon("shotgun"), 1)), 1);
}

// Each weapon found is worth the finding: it kills sooner than the pistol
TEST(Balance, WeaponsFoundKillFasterThanThePistol) {
	const auto seconds_to_kill = [](const WeaponConfig& weapon,
									double distance) {
		return (HitsToKill(SoldierHealth(), Damage(weapon, distance)) - 1) *
			   weapon.attack_speed;
	};
	const double pistol = seconds_to_kill(testing::Weapon("pistol"), kRoom);
	EXPECT_LT(seconds_to_kill(testing::Weapon("mp5"), kRoom), pistol);
	EXPECT_LT(seconds_to_kill(testing::Weapon("shotgun"), 1), pistol);
}

// A level shot (from the eye, half a wall up) meets a soldier below its
// head: a headshot takes aiming up
TEST(Balance, ALevelShotIsNotAHeadshot) {
	const EnemyConfig& soldier = testing::GameData().enemies.at("soldier");
	const double down = 1.0 - 0.5 / soldier.height;
	EXPECT_EQ(soldier.hit_zones.ZoneAt(down), HitZones::Zone::Body);
}

// A soldier across the room must hit a healthy player several times, even
// on the hardest difficulty
TEST(Balance, ThePlayerTakesSeveralHits) {
	const auto& soldier =
		testing::GameData().enemies.find("soldier")->second.weapon;
	for (const auto& difficulty : testing::GameData().difficulties) {
		const double hit =
			difficulty.enemy_damage *
			LinearSlope(soldier.attack_damage, soldier.attack_range, kRoom);
		EXPECT_GE(HitsToKill(100.0, hit), 8) << difficulty.name;
	}
}

// A level carries what its enemies take, with room for misses: the damage
// the weapons carried by then can do (a magazine, the reserve a game starts
// with and the level's ammunition boxes, per weapon) is half again the
// enemies' health, at every difficulty
TEST(Balance, EveryLevelCarriesTheAmmunitionItsEnemiesNeed) {
	const GameConfig& config = testing::GameData();
	std::set<std::string> carried;
	for (const auto& weapon : config.weapons) {
		if (weapon.start) {
			carried.insert(weapon.weapon_name);
		}
	}
	for (const auto& file : config.levels) {
		std::ifstream input(std::string(RESOURCE_DIR) + "levels/" + file);
		const auto level = ParseLevel(input);
		ASSERT_TRUE(level) << file;
		double health = 0.0;
		for (const auto& enemy : level->enemies) {
			health += config.enemies.find(enemy.type)->second.health;
		}
		std::size_t boxes = 0;
		for (const auto& pickup : level->pickups) {
			const auto& found = config.pickups.find(pickup.type)->second;
			boxes += found.effect.ammo_boxes;
			for (std::size_t i = 0; i < config.weapons.size(); ++i) {
				if ((found.effect.weapons >> i & 1U) != 0) {
					carried.insert(config.weapons[i].weapon_name);
				}
			}
		}
		for (const auto& difficulty : config.difficulties) {
			double damage = 0.0;
			for (const auto& weapon : config.weapons) {
				if (!carried.contains(weapon.weapon_name) ||
					weapon.ammo_capacity == 0) {
					continue;
				}
				const double rounds = std::min(
					static_cast<double>(weapon.ammo_capacity +
										weapon.reserve_start) +
						static_cast<double>(boxes * weapon.box_rounds) *
							difficulty.supplies,
					static_cast<double>(weapon.ammo_capacity +
										weapon.reserve_max));
				damage += rounds * Damage(weapon, kRoom);
			}
			EXPECT_GE(damage, 1.5 * health * difficulty.enemy_health)
				<< file << " on " << difficulty.name;
		}
	}
}

}  // namespace
}  // namespace wolfenstein
