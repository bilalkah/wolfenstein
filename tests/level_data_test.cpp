#include "Core/level_data.h"
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace wolfenstein {
namespace {

std::ifstream OpenLevelFile(const std::string& name) {
	return std::ifstream(std::string(RESOURCE_DIR) + "levels/" + name);
}

TEST(LevelData, ParsesTheShippedLevels) {
	auto file = OpenLevelFile("level1.json");
	const auto level = ParseLevel(file);
	ASSERT_TRUE(level) << level.error();
	EXPECT_EQ(level->map, "map1.txt");
	EXPECT_EQ(level->enemies.size(), 10u);
	EXPECT_EQ(level->dynamic_objects.size(), 6u);
	EXPECT_EQ(level->next_level, "level2.json");
	EXPECT_DOUBLE_EQ(level->player.pose.x, 3.0);

	auto last = OpenLevelFile("level2.json");
	const auto level2 = ParseLevel(last);
	ASSERT_TRUE(level2) << level2.error();
	EXPECT_TRUE(level2->next_level.empty());
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

	ASSERT_EQ(config->weapons.size(), 2u);
	const WeaponConfig* shotgun = config->FindWeapon("shotgun");
	ASSERT_NE(shotgun, nullptr);
	EXPECT_EQ(shotgun->ammo_capacity, 2u);
	EXPECT_EQ(shotgun->falloff, DamageFalloff::Exponential);
	EXPECT_EQ(config->FindWeapon("bazooka"), nullptr);
	EXPECT_DOUBLE_EQ(config->light.animation_speed, 0.3);
	EXPECT_DOUBLE_EQ(config->player.translation_speed, 2.0);
}

TEST(LevelData, RejectsAnUnknownDamageFalloff) {
	std::istringstream config(R"({
		"player_config": {"t_speed": 2, "r_speed": 0.4, "width": 0.4, "height": 1},
		"weapons": [{"name": "x", "label": "X", "description": "", "ammo": 1,
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
	EXPECT_TRUE(level->next_level.empty());
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
