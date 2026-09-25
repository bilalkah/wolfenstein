#include "Camera/raycaster.h"
#include "GameMap/map.h"
#include "test_map.h"
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

// A 5x5 room: solid border (wall id 3), empty inside
Map RoomMap() {
	const auto path =
		testing::WriteMapFile("wolfenstein_raycaster_test.txt",
							  {"33333", "30003", "30003", "30003", "33333"});
	return Map(path.string());
}

// Casts a single, almost zero-width ray from the room's centre
Ray CastFromCentre(const Map& map, double theta) {
	RayCaster caster(1, 1e-6, 20.0);
	RayVector rays(1);
	caster.Update(map, Position2D({2.5, 2.5}, theta), rays);
	return rays.front();
}

TEST(RayCaster, HitsWallsAtKnownDistances) {
	const Map map = RoomMap();
	for (const double theta :
		 {0.0, std::numbers::pi / 2, std::numbers::pi, -std::numbers::pi / 2}) {
		const Ray ray = CastFromCentre(map, theta);
		EXPECT_TRUE(ray.is_hit) << "theta=" << theta;
		EXPECT_EQ(ray.wall_id, 3) << "theta=" << theta;
		EXPECT_NEAR(ray.distance, 1.5, 1e-3) << "theta=" << theta;
	}
}

TEST(RayCaster, DiagonalRayTravelsFurther) {
	const Map map = RoomMap();
	const Ray ray = CastFromCentre(map, std::numbers::pi / 4);
	EXPECT_TRUE(ray.is_hit);
	EXPECT_GT(ray.distance, 1.5);
}

}  // namespace
}  // namespace wolfenstein
