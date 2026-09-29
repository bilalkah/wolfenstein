#include "Core/level_data.h"
#include <fstream>
#include <gtest/gtest.h>
#include <numbers>
#include <sstream>
#include <string>

namespace wolfenstein {
namespace {

std::ifstream OpenLevelFile(const std::string& name) {
	return std::ifstream(std::string(RESOURCE_DIR) + "levels/" + name);
}

// The benchmark's level is fixed, so benchmark runs stay comparable
TEST(LevelData, ParsesTheBenchmarkLevel) {
	auto file = OpenLevelFile("benchmark.json");
	const auto level = ParseLevel(file);
	ASSERT_TRUE(level) << level.error();
	EXPECT_EQ(level->map, "map1.txt");
	EXPECT_EQ(level->enemies.size(), 10u);
	EXPECT_EQ(level->dynamic_objects.size(), 6u);
	EXPECT_DOUBLE_EQ(level->player.pose.x, 3.0);
}

TEST(LevelData, ParsesTheGameConfig) {
	auto file = OpenLevelFile("config.json");
	const auto config = ParseGameConfig(file);
	ASSERT_TRUE(config) << config.error();
	ASSERT_TRUE(config->enemies.contains("soldier"));
	const EnemyConfig& soldier = config->enemies.at("soldier");
	// Its body is as it was; its picture as wide as its widest frame (lying
	// dead)
	EXPECT_DOUBLE_EQ(soldier.radius, 0.165);
	EXPECT_GE(soldier.width, 2 * soldier.radius);
	EXPECT_EQ(soldier.weapon.weapon_name, "rifle");
	EXPECT_EQ(config->menu_music, "menu");
	EXPECT_DOUBLE_EQ(soldier.behaviour.follow_range, 5.0);
	// Each fights at its own range: a soldier across a room, a demon up
	// close; every one from where its weapon reaches
	EXPECT_GT(soldier.behaviour.near_range, 0.0);
	EXPECT_LT(soldier.behaviour.near_range, soldier.behaviour.far_range);
	EXPECT_DOUBLE_EQ(config->enemies.at("demon").behaviour.near_range, 0.0);
	// Soldiers step aside between shots; the minigun zombie stands planted
	EXPECT_GT(soldier.behaviour.sidestep, 0.0);
	EXPECT_DOUBLE_EQ(config->enemies.at("minigun_zombie").behaviour.sidestep,
					 0.0);
	// Each may drop ammunition or health, by chance; a boss always drops
	ASSERT_FALSE(soldier.drops.empty());
	EXPECT_EQ(soldier.drops.front().pickup, "clip");
	EXPECT_LT(soldier.drops.front().chance, 1.0);
	for (const EnemyDrop& drop : config->enemies.at("cyber_demon").drops) {
		EXPECT_DOUBLE_EQ(drop.chance, 1.0) << drop.pickup;
	}
	for (const auto& [type, enemy] : config->enemies) {
		for (const EnemyDrop& drop : enemy.drops) {
			EXPECT_TRUE(config->pickups.contains(drop.pickup)) << type;
		}
	}
	// Badly hurt, a soldier breaks off for cover; the demons and the bosses
	// fight to the end
	EXPECT_GT(soldier.behaviour.retreat_below, 0.0);
	for (const char* type :
		 {"demon", "caco_demon", "cyber_demon", "minigun_zombie"}) {
		EXPECT_DOUBLE_EQ(config->enemies.at(type).behaviour.retreat_below, 0.0)
			<< type;
	}
	for (const auto& [type, enemy] : config->enemies) {
		EXPECT_LE(enemy.behaviour.far_range, enemy.weapon.attack_range) << type;
	}

	// The arsenal, in slot order: a pistol to start with, the rest found
	ASSERT_EQ(config->weapons.size(), 7u);
	EXPECT_EQ(config->weapons[0].weapon_name, "pistol");
	EXPECT_TRUE(config->weapons[0].start);
	for (std::size_t i = 1; i < config->weapons.size(); ++i) {
		EXPECT_FALSE(config->weapons[i].start) << i;
	}
	// A double-barrelled shotgun: both barrels at once, broken open to load
	// again straight after, with the sound of it
	const WeaponConfig* both = config->FindWeapon("super_shotgun");
	ASSERT_NE(both, nullptr);
	EXPECT_EQ(both->ammo_capacity, 1u);
	EXPECT_TRUE(both->reload_after_shot);
	EXPECT_EQ(both->reload_sound, SoundEffect::SuperShotgunReload);
	// A saw: melee, that starts up as it comes up and cuts with its own
	// sound
	const WeaponConfig* saw = config->FindWeapon("chainsaw");
	ASSERT_NE(saw, nullptr);
	EXPECT_EQ(saw->ammo_capacity, 0u);
	EXPECT_EQ(saw->raise_sound, SoundEffect::SawUp);
	EXPECT_EQ(saw->shot_sound, SoundEffect::Saw);
	EXPECT_EQ(saw->hit_sound, SoundEffect::SawHit);
	EXPECT_FALSE(config->weapons[0].hit_sound) << "a gun sounds its shot";
	// A rocket launcher: its rockets fly, and burst with a blast
	const WeaponConfig* launcher = config->FindWeapon("rocket_launcher");
	ASSERT_NE(launcher, nullptr);
	ASSERT_TRUE(launcher->projectile);
	const ProjectileConfig rocket =
		launcher->projectile.value_or(ProjectileConfig{});
	EXPECT_EQ(rocket.name, "rocket");
	EXPECT_GT(rocket.speed, 0.0);
	EXPECT_GT(rocket.splash_radius, 0.0);
	EXPECT_GT(rocket.splash_damage.first, rocket.splash_damage.second);
	EXPECT_EQ(rocket.burst_sound, SoundEffect::RocketBurst);
	EXPECT_GT(rocket.width, 0.0) << "sized from its art";
	// A plasma rifle: its bolts fly, and burst with none
	const WeaponConfig* plasma = config->FindWeapon("plasma_rifle");
	ASSERT_NE(plasma, nullptr);
	ASSERT_TRUE(plasma->projectile);
	EXPECT_DOUBLE_EQ(
		plasma->projectile.value_or(ProjectileConfig{}).splash_radius, 0.0);
	EXPECT_FALSE(config->weapons[0].projectile)
		<< "a pistol shot strikes at once";
	// A weapon pickup gives its weapon, as the bit of its slot
	ASSERT_TRUE(config->pickups.contains("mp5"));
	EXPECT_EQ(config->pickups.at("mp5").effect.weapons, 1U << 1);
	const WeaponConfig* shotgun = config->FindWeapon("shotgun");
	ASSERT_NE(shotgun, nullptr);
	EXPECT_EQ(shotgun->ammo_capacity, 2u);
	EXPECT_FALSE(shotgun->reload_after_shot);
	EXPECT_EQ(shotgun->falloff, DamageFalloff::Exponential);
	EXPECT_EQ(config->FindWeapon("bazooka"), nullptr);
	EXPECT_DOUBLE_EQ(config->light.animation_speed, 0.3);
	EXPECT_DOUBLE_EQ(config->player.translation_speed, 2.0);

	// Rounds carried besides the magazine, and what the pickups give
	const WeaponConfig* mp5 = config->FindWeapon("mp5");
	ASSERT_NE(mp5, nullptr);
	EXPECT_EQ(mp5->reserve_start, 60u);
	EXPECT_EQ(mp5->reserve_max, 180u);
	EXPECT_EQ(mp5->box_rounds, 30u);
	// Each gun sounds its own
	EXPECT_EQ(config->weapons[0].shot_sound, SoundEffect::PistolShot);
	EXPECT_EQ(mp5->shot_sound, SoundEffect::SmgShot);
	EXPECT_EQ(shotgun->shot_sound, SoundEffect::Shotgun);
	// A shotgun blast is a fan of pellets
	EXPECT_EQ(shotgun->pellets, 7u);
	EXPECT_NEAR(shotgun->spread, 8.0 * std::numbers::pi / 180.0, 1e-12);
	EXPECT_EQ(mp5->pellets, 1u);
	// Each enemy type has its own health
	EXPECT_DOUBLE_EQ(config->enemies.at("soldier").health, 60.0);
	// Each has its own voice and weapon's sound; one that names none sounds
	// as every enemy
	EXPECT_EQ(soldier.sounds.alert, SoundEffect::EnemyAlert);
	EXPECT_EQ(soldier.sounds.attack, SoundEffect::NpcAttack);
	ASSERT_TRUE(config->enemies.contains("demon"));
	const EnemyConfig& demon = config->enemies.at("demon");
	EXPECT_EQ(demon.sounds.attack, SoundEffect::DemonAttack);
	EXPECT_EQ(demon.sounds.alert, SoundEffect::DemonAlert);
	EXPECT_EQ(demon.sounds.pain, SoundEffect::DemonPain);
	EXPECT_EQ(demon.sounds.death, SoundEffect::DemonDeath);
	ASSERT_TRUE(config->enemies.contains("shotgun_zombie"));
	EXPECT_EQ(config->enemies.at("shotgun_zombie").sounds.attack,
			  SoundEffect::Shotgun);
	EXPECT_EQ(config->enemies.at("shotgun_zombie").sounds.pain,
			  SoundEffect::NpcPain);
	ASSERT_TRUE(config->enemies.contains("minigun_zombie"));
	EXPECT_GT(config->enemies.at("minigun_zombie").health,
			  config->enemies.at("shotgun_zombie").health)
		<< "a mini-boss";
	// Where a shot lands on a figure: the head hurts twice as much
	EXPECT_DOUBLE_EQ(config->enemies.at("soldier").hit_zones.head_damage, 2.0);
	EXPECT_DOUBLE_EQ(config->enemies.at("soldier").hit_zones.leg_damage, 0.6);
	ASSERT_TRUE(config->pickups.contains("medkit"));
	EXPECT_DOUBLE_EQ(config->pickups.at("medkit").effect.health, 25.0);
	EXPECT_EQ(config->pickups.at("medkit").effect.ammo_boxes, 0u);
	ASSERT_EQ(config->difficulties.size(), 3u);
	EXPECT_EQ(config->difficulties.front().name, "easy");
	ASSERT_NE(config->FindDifficulty("hard"), nullptr);
	EXPECT_GT(config->FindDifficulty("hard")->enemy_damage, 1.0);
	// The harder, the more enemies shoot at once
	EXPECT_LT(config->FindDifficulty("easy")->attackers,
			  config->FindDifficulty("normal")->attackers);
	EXPECT_LT(config->FindDifficulty("normal")->attackers,
			  config->FindDifficulty("hard")->attackers);
	EXPECT_EQ(config->FindDifficulty("impossible"), nullptr);
	ASSERT_TRUE(config->pickups.contains("ammo_box"));
	EXPECT_EQ(config->pickups.at("ammo_box").effect.ammo_boxes, 1u);
	EXPECT_DOUBLE_EQ(config->pickups.at("ammo_box").effect.health, 0.0);
}

TEST(LevelData, RejectsAnUnknownDamageFalloff) {
	std::istringstream config(R"({
		"player_config": {"t_speed": 2, "r_speed": 0.4, "width": 0.4, "height": 1},
		"weapons": [{"name": "x", "label": "X", "description": "", "ammo": 1,
					 "reserve": {"start": 0, "max": 1, "box": 1},
					 "damage": [1, 1], "range": 1, "attack_speed": 1,
					 "reload_speed": 1, "falloff": "quadratic"}],
		"config_enemy": {},
		"config_dynamic": {"light": {"animation_speed": 1, "width": 1, "height": 1}}})");
	const auto parsed = ParseGameConfig(config);
	ASSERT_FALSE(parsed);
	EXPECT_NE(parsed.error().find("quadratic"), std::string::npos);
}

TEST(LevelData, RejectsAnUnknownEnemySound) {
	std::istringstream config(R"({
		"player_config": {"t_speed": 2, "r_speed": 0.4, "width": 0.4, "height": 1},
		"weapons": [],
		"config_enemy": {"x": {"t_speed": 1, "r_speed": 1, "width": 1,
							   "height": 1, "health": 1,
							   "ai": {"idle_frame_seconds": 1, "follow_range": 1},
							   "weapon": {"name": "x", "damage": [1, 1],
										  "range": 1, "attack_speed": 1,
										  "attack_rate": 1},
							   "sounds": {"alert": "moo"}}},
		"config_dynamic": {"light": {"animation_speed": 1, "width": 1, "height": 1}}})");
	const auto parsed = ParseGameConfig(config);
	ASSERT_FALSE(parsed);
	EXPECT_NE(parsed.error().find("moo"), std::string::npos) << parsed.error();
}

// A drop's chance is from 0 to 1
TEST(LevelData, RejectsAnImpossibleChance) {
	std::istringstream config(R"({
		"player_config": {"t_speed": 2, "r_speed": 0.4, "width": 0.4, "height": 1},
		"weapons": [],
		"config_enemy": {"x": {"t_speed": 1, "r_speed": 1, "width": 1,
							   "height": 1, "health": 1,
							   "ai": {"idle_frame_seconds": 1, "follow_range": 1},
							   "drops": [{"pickup": "clip", "chance": 1.5}],
							   "weapon": {"name": "x", "damage": [1, 1],
										  "range": 1, "attack_speed": 1,
										  "attack_rate": 1}}},
		"config_dynamic": {"light": {"animation_speed": 1, "width": 1, "height": 1}}})");
	const auto parsed = ParseGameConfig(config);
	ASSERT_FALSE(parsed);
	EXPECT_NE(parsed.error().find("chance"), std::string::npos)
		<< parsed.error();
}

// An enemy's range is [near, far]: nearer than far
TEST(LevelData, RejectsABackwardsRange) {
	std::istringstream config(R"({
		"player_config": {"t_speed": 2, "r_speed": 0.4, "width": 0.4, "height": 1},
		"weapons": [],
		"config_enemy": {"x": {"t_speed": 1, "r_speed": 1, "width": 1,
							   "height": 1, "health": 1,
							   "ai": {"idle_frame_seconds": 1, "follow_range": 1,
									  "range": [3, 1]},
							   "weapon": {"name": "x", "damage": [1, 1],
										  "range": 1, "attack_speed": 1,
										  "attack_rate": 1}}},
		"config_dynamic": {"light": {"animation_speed": 1, "width": 1, "height": 1}}})");
	const auto parsed = ParseGameConfig(config);
	ASSERT_FALSE(parsed);
	EXPECT_NE(parsed.error().find("range"), std::string::npos)
		<< parsed.error();
}

TEST(LevelData, ReportsMalformedFilesAsErrors) {
	std::istringstream syntax("{ \"map\": ");
	EXPECT_FALSE(ParseLevel(syntax));

	// A missing key is an error, not a null silently read as 0
	std::istringstream missing(
		R"({"map": "m.txt", "enemies": [], "dynamicObjects": []})");
	const auto no_player = ParseLevel(missing);
	ASSERT_FALSE(no_player);
	EXPECT_NE(no_player.error().find("player"), std::string::npos);

	std::istringstream wrong_type(
		R"({"map": 7, "player": {"position": {"x": 1, "y": 1, "theta": 0}},
		    "enemies": [], "dynamicObjects": []})");
	EXPECT_FALSE(ParseLevel(wrong_type));
}

// The reader streams the file into LevelData: fields the game does not
// read are skipped, and a missing field is reported by name
TEST(LevelData, IgnoresUnknownFields) {
	std::istringstream input(R"({
		"map": "m.txt", "author": {"name": "someone", "tags": [1, true, null]},
		"player": {"position": {"x": 1, "y": 2, "theta": 0.5}, "hat": "red"},
		"enemies": [{"type": "soldier", "position": {"x": 3, "y": 4, "theta": 0}}],
		"dynamicObjects": [{"type": "red_light", "position": {"x": 5, "y": 6}}],
		"staticObjects": []})");
	const auto level = ParseLevel(input);
	ASSERT_TRUE(level) << level.error();
	EXPECT_EQ(level->map, "m.txt");
	EXPECT_DOUBLE_EQ(level->player.theta, 0.5);
	ASSERT_EQ(level->enemies.size(), 1u);
	EXPECT_DOUBLE_EQ(level->enemies[0].position.pose.y, 4.0);
	ASSERT_EQ(level->dynamic_objects.size(), 1u);
	EXPECT_EQ(level->dynamic_objects[0].type, "red_light");
}

// Pickups are optional in a level; each has a type and a place
TEST(LevelData, ParsesPickups) {
	std::istringstream input(R"({
		"map": "m.txt", "player": {"position": {"x": 1, "y": 2, "theta": 0}},
		"enemies": [], "dynamicObjects": [],
		"pickups": [{"type": "medkit", "position": {"x": 3.5, "y": 4.5}},
					{"type": "ammo_box", "position": {"x": 5.5, "y": 6.5}}]})");
	const auto level = ParseLevel(input);
	ASSERT_TRUE(level) << level.error();
	ASSERT_EQ(level->pickups.size(), 2u);
	EXPECT_EQ(level->pickups[0].type, "medkit");
	EXPECT_DOUBLE_EQ(level->pickups[1].position.y, 6.5);
	EXPECT_TRUE(level->dynamic_objects.empty());

	std::istringstream placeless(R"({
		"map": "m.txt", "player": {"position": {"x": 1, "y": 2, "theta": 0}},
		"enemies": [], "dynamicObjects": [],
		"pickups": [{"type": "medkit"}]})");
	const auto missing = ParseLevel(placeless);
	ASSERT_FALSE(missing);
	EXPECT_NE(missing.error().find("pickup position"), std::string::npos)
		<< missing.error();
}

// An enemy's patrol: how far from its post it walks about; one without
// stands guard
TEST(LevelData, ParsesPatrolRadii) {
	std::istringstream input(R"({
		"map": "m.txt", "player": {"position": {"x": 1, "y": 2, "theta": 0}},
		"enemies": [
			{"type": "soldier", "position": {"x": 3.5, "y": 4.5, "theta": 0},
			 "patrol_radius": 3.5},
			{"type": "soldier", "position": {"x": 9.5, "y": 9.5, "theta": 1}}],
		"dynamicObjects": []})");
	const auto level = ParseLevel(input);
	ASSERT_TRUE(level) << level.error();
	ASSERT_EQ(level->enemies.size(), 2u);
	EXPECT_DOUBLE_EQ(level->enemies[0].patrol_radius, 3.5);
	EXPECT_DOUBLE_EQ(level->enemies[0].position.pose.y, 4.5) << "untouched";
	EXPECT_DOUBLE_EQ(level->enemies[1].patrol_radius, 0.0);

	std::istringstream negative(R"({
		"map": "m.txt", "player": {"position": {"x": 1, "y": 2, "theta": 0}},
		"enemies": [{"type": "soldier",
					 "position": {"x": 3.5, "y": 4.5, "theta": 0},
					 "patrol_radius": -1}],
		"dynamicObjects": []})");
	EXPECT_FALSE(ParseLevel(negative));
}

TEST(LevelData, AnEnemyNeedsAFullPosition) {
	std::istringstream input(R"({
		"map": "m.txt", "player": {"position": {"x": 1, "y": 2, "theta": 0}},
		"enemies": [{"type": "soldier", "position": {"x": 3, "y": 4}}],
		"dynamicObjects": []})");
	const auto level = ParseLevel(input);
	ASSERT_FALSE(level);
	EXPECT_NE(level.error().find("theta"), std::string::npos) << level.error();
}

}  // namespace
}  // namespace wolfenstein
