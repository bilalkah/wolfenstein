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
	EXPECT_DOUBLE_EQ(soldier.width, 0.3);
	EXPECT_EQ(soldier.weapon.weapon_name, "rifle");
	EXPECT_DOUBLE_EQ(soldier.behaviour.follow_range, 5.0);

	// The arsenal, in slot order: a pistol to start with
	ASSERT_EQ(config->weapons.size(), 3u);
	EXPECT_EQ(config->weapons[0].weapon_name, "pistol");
	EXPECT_TRUE(config->weapons[0].start);
	EXPECT_FALSE(config->weapons[1].start);
	EXPECT_FALSE(config->weapons[2].start);
	// A weapon pickup gives its weapon, as the bit of its slot
	ASSERT_TRUE(config->pickups.contains("mp5"));
	EXPECT_EQ(config->pickups.at("mp5").effect.weapons, 1U << 1);
	const WeaponConfig* shotgun = config->FindWeapon("shotgun");
	ASSERT_NE(shotgun, nullptr);
	EXPECT_EQ(shotgun->ammo_capacity, 2u);
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
	// A shotgun blast is a fan of pellets
	EXPECT_EQ(shotgun->pellets, 7u);
	EXPECT_NEAR(shotgun->spread, 8.0 * std::numbers::pi / 180.0, 1e-12);
	EXPECT_EQ(mp5->pellets, 1u);
	// Each enemy type has its own health
	EXPECT_DOUBLE_EQ(config->enemies.at("soldier").health, 60.0);
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
