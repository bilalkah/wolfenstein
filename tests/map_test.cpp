#include "Map/map.h"
#include "test_map.h"
#include <gtest/gtest.h>

namespace wolfenstein {
namespace {

TEST(Map, LoadsSizeAndCells) {
	const auto path = testing::WriteMapFile("wolfenstein_map_test.txt",
											{"333", "302", "333"});
	const Map map(path.string());

	ASSERT_EQ(map.GetSizeX(), 3);
	ASSERT_EQ(map.GetSizeY(), 3);
	EXPECT_EQ(map[0][0], 3);
	EXPECT_EQ(map[1][1], 0);
	EXPECT_EQ(map[1][2], 2);
}

TEST(Map, LoadsLevelOneMap) {
	const Map map(std::string(RESOURCE_DIR) + "maps/map1.txt");

	ASSERT_EQ(map.GetSizeX(), 26);
	ASSERT_EQ(map.GetSizeY(), 16);
	EXPECT_EQ(map[3][1], 0);  // the player's spawn cell
	EXPECT_EQ(map[3][3], 1);
	for (uint16_t y = 0; y < map.GetSizeY(); ++y) {
		EXPECT_NE(map[0][y], 0) << "top border is solid at y=" << y;
	}
}

}  // namespace
}  // namespace wolfenstein
