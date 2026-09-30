// A level counts how the player did in it: enemies killed, supplies taken,
// how much of it they explored and how long they took

#include "Camera/camera.h"
#include "Core/scene.h"
#include "ShootingManager/shooting_manager.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// A corridor of five open cells: the player at one end, a soldier at the
// other, a medkit and an ammo box between them
class LevelStatsTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1, .pickups = 2};

	LevelStatsTest()
		: map_(testing::WriteMapFile("karakale_stats_test.txt",
									 {"3333333", "3000003", "3333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound()) {
		scene_.SetPlayer(player_);
		EXPECT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
									Position2D({1.5, 5.5}, -kFacingDown)));
		for (const auto& [type, y] :
			 {std::pair{"medkit", 2.5}, std::pair{"ammo_box", 3.5}}) {
			const PickupConfig& pickup =
				testing::GameData().pickups.find(type)->second;
			EXPECT_TRUE(scene_.AddPickup(
				{1.5, y}, testing::TestTextures().GetTextureId(pickup.texture),
				pickup.width, pickup.height, pickup.effect));
		}
		scene_.FinishLoading();
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(LevelStatsTest, ALevelStartsWithNothingDone) {
	const LevelStats stats = scene_.GetStats();
	EXPECT_EQ(stats.kills, 0u);
	EXPECT_EQ(stats.enemies, 1u);
	EXPECT_EQ(stats.pickups_taken, 0u);
	EXPECT_EQ(stats.pickups, 2u);
	EXPECT_EQ(stats.explored_percent, 0);
	EXPECT_DOUBLE_EQ(stats.seconds, 0.0);
}

TEST_F(LevelStatsTest, CountsKillsAndSuppliesTaken) {
	scene_.GetPickups()[1]->Take();
	const Weapon weapon(testing::Weapon("mp5"), testing::TestTextures(),
						testing::TestSound());
	for (int shot = 0; shot < 40; ++shot) {
		ResolvePlayerShot(scene_, weapon, player_.GetPosition());
	}
	const LevelStats stats = scene_.GetStats();
	EXPECT_EQ(stats.kills, 1u);
	EXPECT_EQ(stats.pickups_taken, 1u);
}

// Only cells a character can stand in count: seeing walls adds nothing
TEST_F(LevelStatsTest, ExploredCountsTheOpenCellsSeen) {
	scene_.Explore(0, 0);  // a wall
	EXPECT_EQ(scene_.GetStats().explored_percent, 0);
	scene_.Explore(1, 1);
	scene_.Explore(1, 2);
	scene_.Explore(1, 2);								// twice is still once
	EXPECT_EQ(scene_.GetStats().explored_percent, 40);	// 2 of 5

	Camera2D camera(Camera2DConfig(320, std::numbers::pi / 3, 15.0));
	camera.SetScene(scene_);
	camera.Update(player_.GetPosition(), 1.0);
	camera.ExploreView();
	EXPECT_EQ(scene_.GetStats().explored_percent, 100);
}

// The clock runs while enemies are left and stops with the last of them
TEST_F(LevelStatsTest, TheClockStopsWhenTheLevelIsCleared) {
	for (int tick = 0; tick < 60; ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_NEAR(scene_.GetStats().seconds, 1.0, 1e-9);
	scene_.DecreaseAliveEnemies();
	for (int tick = 0; tick < 60; ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_NEAR(scene_.GetStats().seconds, 1.0, 1e-9);
}

}  // namespace
}  // namespace karakale
