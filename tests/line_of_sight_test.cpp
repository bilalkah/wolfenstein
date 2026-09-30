#include "Camera/single_raycaster.h"
#include "GameMap/map.h"
#include "test_map.h"
#include <gtest/gtest.h>

namespace karakale {
namespace {

// Line of sight from `from` to `to` on the given map
bool CanSee(std::initializer_list<const char*> rows, vector2d from,
			vector2d to) {
	const Map map(
		testing::WriteMapFile("karakale_los_test.txt", rows).string());
	return CastLineOfSight(map, from, to).is_hit;
}

TEST(LineOfSight, SeesAcrossAnEmptyRoom) {
	EXPECT_TRUE(CanSee({"33333", "30003", "30003", "30003", "33333"},
					   {1.5, 1.5}, {3.5, 1.5}));
}

TEST(LineOfSight, WallBlocksTheView) {
	EXPECT_FALSE(CanSee({"33333", "30303", "30303", "30303", "33333"},
						{1.5, 1.5}, {1.5, 3.5}));
}

// The ray used to loop forever once it left a map without a solid border:
// cells outside the map are now blocked, so it stops at the edge
TEST(LineOfSight, TerminatesWhenTheRayLeavesTheMap) {
	EXPECT_FALSE(CanSee({"000", "000", "000"}, {0.5, 0.5}, {8.5, 0.5}));
}

}  // namespace
}  // namespace karakale
