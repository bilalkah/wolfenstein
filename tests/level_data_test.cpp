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
	EXPECT_DOUBLE_EQ(config->enemies.at("soldier").width, 0.3);
	EXPECT_DOUBLE_EQ(config->light.animation_speed, 0.3);
	EXPECT_DOUBLE_EQ(config->player.translation_speed, 2.0);
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

}  // namespace
}  // namespace wolfenstein
