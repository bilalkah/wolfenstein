#include "Camera/raycaster.h"
#include "GameMap/map.h"
#include "test_map.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <numbers>
#include <string>
#include <vector>

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

// A 41x41 room, to face one long wall with the whole view
Map HallMap() {
	std::vector<std::string> rows(41, "3" + std::string(39, '0') + "3");
	rows.front() = rows.back() = std::string(41, '3');
	const auto path =
		testing::WriteMapFile("wolfenstein_raycaster_hall_test.txt", rows);
	return Map(path.string());
}

constexpr double kWidestView = 80.0 * std::numbers::pi / 180.0;

// The rays cross a flat camera plane at even steps, so they meet a flat wall
// facing the eye at even steps too, however wide the view: the wall is drawn
// straight, where rays at equal angles bunched in the middle and spread at
// the edges, bending it
TEST(RayCaster, FacingWallIsMetAtEvenSteps) {
	const Map map = HallMap();
	constexpr int kRays = 60;
	RayCaster caster(kRays, kWidestView, 50.0);
	RayVector rays(kRays);
	caster.Update(map, Position2D({20.5, 20.5}, 0.0), rays);

	const double step = rays[1].hit_point.y - rays[0].hit_point.y;
	EXPECT_GT(step, 0.0);
	for (std::size_t i = 0; i + 1 < rays.size(); ++i) {
		ASSERT_TRUE(rays[i].is_hit) << i;
		EXPECT_NEAR(rays[i].hit_point.x, rays[0].hit_point.x, 1e-9) << i;
		EXPECT_NEAR(rays[i + 1].hit_point.y - rays[i].hit_point.y, step, 1e-9)
			<< i;
	}
}

// Sprites are placed by the rays' own projection: each ray's direction falls
// at its own column, and the view's edges at the screen's
TEST(RayCaster, AcrossPlacesEachRayAtItsColumn) {
	const Map map = HallMap();
	constexpr int kRays = 60;
	RayCaster caster(kRays, kWidestView, 50.0);
	RayVector rays(kRays);
	const double facing = 0.3;
	caster.Update(map, Position2D({20.5, 20.5}, facing), rays);

	for (std::size_t i = 0; i < rays.size(); ++i) {
		EXPECT_NEAR(caster.Across(rays[i].theta - facing),
					2.0 * static_cast<double>(i) / kRays - 1.0, 1e-9)
			<< i;
	}
	EXPECT_NEAR(caster.Across(-kWidestView / 2), -1.0, 1e-12);
	EXPECT_NEAR(caster.Across(kWidestView / 2), 1.0, 1e-12);
	EXPECT_DOUBLE_EQ(caster.Across(0.0), 0.0);
	// Behind the eye: far off to that side, never back on the screen
	EXPECT_LT(caster.Across(-2.0), -100.0);
	EXPECT_GT(caster.Across(2.0), 100.0);
}

}  // namespace
}  // namespace wolfenstein
