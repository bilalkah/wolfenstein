#include "GameMap/map.h"
#include "test_map.h"
#include <gtest/gtest.h>

namespace karakale {
namespace {

TEST(Map, LoadsSizeAndCells) {
	const auto path =
		testing::WriteMapFile("karakale_map_test.txt", {"333", "302", "333"});
	const Map map(path.string());

	ASSERT_EQ(map.GetSizeX(), 3);
	ASSERT_EQ(map.GetSizeY(), 3);
	EXPECT_EQ((map.GetCells()[0, 0]), 3);
	EXPECT_EQ((map.GetCells()[1, 1]), 0);
	EXPECT_EQ((map.GetCells()[1, 2]), 2);
}

TEST(Map, LoadsLevelOneMap) {
	const Map map(std::string(RESOURCE_DIR) + "maps/map1.txt");

	ASSERT_EQ(map.GetSizeX(), 26);
	ASSERT_EQ(map.GetSizeY(), 16);
	EXPECT_EQ((map.GetCells()[3, 1]), 0);  // the player's spawn cell
	EXPECT_EQ((map.GetCells()[3, 3]), 1);
	for (uint16_t y = 0; y < map.GetSizeY(); ++y) {
		EXPECT_NE((map.GetCells()[0, y]), 0)
			<< "top border is solid at y=" << y;
	}
}

TEST(Map, RejectsMalformedFiles) {
	const auto ragged = testing::WriteMapFile("karakale_map_ragged_test.txt",
											  {"333", "30", "333"});
	const auto ragged_map = Map::FromFile(ragged.string());
	ASSERT_FALSE(ragged_map);
	EXPECT_NE(ragged_map.error().find("row 1"), std::string::npos);

	const auto unknown = testing::WriteMapFile("karakale_map_unknown_test.txt",
											   {"333", "3x3", "333"});
	EXPECT_FALSE(Map::FromFile(unknown.string()));

	EXPECT_FALSE(Map::FromFile("/nonexistent/karakale_map.txt"));
}

TEST(Map, CellsOutsideTheMapAreBlocked) {
	const auto path = testing::WriteMapFile("karakale_map_bounds_test.txt",
											{"000", "010", "000"});
	const Map map(path.string());

	EXPECT_FALSE(map.IsBlocked(0, 0));
	EXPECT_TRUE(map.IsBlocked(1, 1));  // wall
	EXPECT_TRUE(map.IsBlocked(-1, 0));
	EXPECT_TRUE(map.IsBlocked(0, 3));
	EXPECT_TRUE(map.IsBlocked(3, 0));
}

TEST(Map, PositionsAreFlooredNotTruncated) {
	const auto path = testing::WriteMapFile("karakale_map_floor_test.txt",
											{"000", "000", "000"});
	const Map map(path.string());

	EXPECT_FALSE(map.IsBlocked(vector2d{0.5, 0.5}));
	EXPECT_FALSE(map.IsBlocked(vector2d{2.99, 2.99}));
	// Truncation would read cell (0, 0) for these and call them free
	EXPECT_TRUE(map.IsBlocked(vector2d{-0.5, 0.5}));
	EXPECT_TRUE(map.IsBlocked(vector2d{0.5, -0.5}));
}

}  // namespace
}  // namespace karakale
