// The map shows only what the player has seen: the camera marks the cells
// its view reaches, and nothing else

#include "Camera/camera.h"
#include "Core/scene.h"
#include "Graphics/minimap.h"
#include "Graphics/renderer_interface.h"
#include "Profiler/profiler.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace karakale {
namespace {

constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// Two corridors along y, at x = 1 and x = 3, with a wall between them
class ExplorationTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.pickups = 3};

	ExplorationTest()
		: map_(testing::WriteMapFile(
				   "karakale_exploration_test.txt",
				   {"33333333", "30000003", "33333333", "30000003", "33333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_) {
		camera_.SetScene(scene_);
	}

	void Look(const Position2D& eye) {
		camera_.Update(eye, 1.0);
		camera_.ExploreView();
	}

	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Camera2D camera_{Camera2DConfig(320, std::numbers::pi / 3, 15.0)};
};

TEST_F(ExplorationTest, ALevelStartsUnexplored) {
	const int size_x = map_.GetSizeX();
	const int size_y = map_.GetSizeY();
	for (int x = 0; x < size_x; ++x) {
		for (int y = 0; y < size_y; ++y) {
			EXPECT_FALSE(scene_.IsExplored(x, y)) << x << "," << y;
		}
	}
}

TEST_F(ExplorationTest, WhatTheViewReachesIsExplored) {
	Look(Position2D({1.5, 1.5}, kFacingDown));
	// The corridor ahead, and the wall that ends it
	for (int y = 1; y <= 6; ++y) {
		EXPECT_TRUE(scene_.IsExplored(1, y)) << "floor at y " << y;
	}
	EXPECT_TRUE(scene_.IsExplored(1, 7));
	// The walls along it
	EXPECT_TRUE(scene_.IsExplored(0, 4));
	EXPECT_TRUE(scene_.IsExplored(2, 4));
	// The corridor behind the wall is unseen
	for (int y = 1; y <= 6; ++y) {
		EXPECT_FALSE(scene_.IsExplored(3, y)) << "hidden floor at y " << y;
	}
}

TEST_F(ExplorationTest, CellsOutsideTheMapAreIgnored) {
	scene_.Explore(-1, 0);
	scene_.Explore(0, 100);
	EXPECT_FALSE(scene_.IsExplored(-1, 0));
	EXPECT_FALSE(scene_.IsExplored(0, 100));
}

#ifdef KARAKALE_COUNTS_ALLOCATIONS
TEST_F(ExplorationTest, ExploringAllocatesNothing) {
	Look(Position2D({1.5, 1.5}, kFacingDown));	// the camera's first frame
	const auto before = AllocationStats::count;
	for (int turn = 0; turn < 16; ++turn) {
		Look(Position2D({1.5, 1.5 + turn * 0.3}, turn * 0.4));
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}

// Drawn every frame over the game: small or large, it draws from storage
// sized once
TEST_F(ExplorationTest, DrawingTheMapAllocatesNothing) {
	Camera2D view(Camera2DConfig(1280, 1.0, 20.0));
	RendererContext context(
		"map test", RenderConfig(1280, 720, 0, 32, 0, 20.0, 1.0, false), view);
	Minimap minimap(context);
	minimap.SetScene(scene_);
	// Health and ammunition lying about, and one already taken
	for (const auto& [type, where] :
		 {std::pair{"medkit", vector2d{1.5, 3.5}},
		  std::pair{"ammo_box", vector2d{3.5, 3.5}},
		  std::pair{"ammo_box", vector2d{3.5, 5.5}}}) {
		const PickupConfig& pickup =
			testing::GameData().pickups.find(type)->second;
		ASSERT_TRUE(scene_.AddPickup(
			where, testing::TestTextures().GetTextureId(pickup.texture),
			pickup.width, pickup.height, pickup.effect));
	}
	scene_.GetPickups().back()->Take();
	const int size_x = map_.GetSizeX();
	const int size_y = map_.GetSizeY();
	for (int x = 0; x < size_x; ++x) {
		for (int y = 0; y < size_y; ++y) {
			scene_.Explore(x, y);
		}
	}
	const Position2D player({1.5, 1.5}, kFacingDown);
	minimap.Render(player, false);	// the first frame
	const auto before = AllocationStats::count;
	for (int frame = 0; frame < 60; ++frame) {
		minimap.Render(player, frame % 2 == 0);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace karakale
