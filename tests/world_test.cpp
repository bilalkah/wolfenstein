// A World owns everything the game simulates and nothing in it is global,
// so a test can build one without a window or an audio device and play it
// through level loads.

#include "Core/world.h"
#include "Camera/camera.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <memory>

namespace wolfenstein {
namespace {

std::unique_ptr<World> MakeWorld() {
	auto loader = SceneLoader::Open(RESOURCE_DIR);
	EXPECT_TRUE(loader) << loader.error();
	return std::make_unique<World>(testing::TestTextures(), std::move(*loader),
								   std::make_unique<SoundManager>());
}

std::shared_ptr<Camera2D> MakeCamera() {
	return std::make_shared<Camera2D>(Camera2DConfig(64, 1.0, 10.0));
}

TEST(World, NewGameLoadsTheFirstLevelWithThePlayerInIt) {
	auto world = MakeWorld();
	auto camera = MakeCamera();
	ASSERT_FALSE(world->HasLevel());

	const auto started = world->NewGame("mp5", camera);
	ASSERT_TRUE(started) << started.error();
	Scene& level = world->CurrentLevel();
	EXPECT_EQ(level.GetEnemies().size(), 10u);
	EXPECT_EQ(&level.GetPlayer(), &world->GetPlayer());
	EXPECT_DOUBLE_EQ(world->GetPlayer().GetPosition().pose.x, 3.0);
	EXPECT_TRUE(world->HasNextLevel());
}

// The path that swaps levels while the game runs: the next level replaces
// the finished one and the player moves into it
TEST(World, NextLevelReplacesTheFinishedOne) {
	auto world = MakeWorld();
	auto camera = MakeCamera();
	ASSERT_TRUE(world->NewGame("shotgun", camera));

	const auto next = world->NextLevel();
	ASSERT_TRUE(next) << next.error();
	EXPECT_EQ(world->CurrentLevel().GetEnemies().size(), 15u);
	EXPECT_EQ(&world->CurrentLevel().GetPlayer(), &world->GetPlayer());
	EXPECT_FALSE(world->HasNextLevel());  // level 2 is the last
}

// A new game replaces both the level and the player it borrows
TEST(World, NewGameStartsOver) {
	auto world = MakeWorld();
	auto camera = MakeCamera();
	ASSERT_TRUE(world->NewGame("mp5", camera));
	ASSERT_TRUE(world->NextLevel());

	ASSERT_TRUE(world->NewGame("mp5", camera));
	EXPECT_EQ(world->CurrentLevel().GetEnemies().size(), 10u);
	EXPECT_TRUE(world->HasNextLevel());
}

// Nothing is shared between worlds: two can run side by side (a server and
// a client, later)
TEST(World, WorldsAreIndependent) {
	auto first = MakeWorld();
	auto second = MakeWorld();
	auto camera = MakeCamera();
	ASSERT_TRUE(first->NewGame("mp5", camera));
	ASSERT_TRUE(second->NewGame("mp5", camera));
	ASSERT_TRUE(second->NextLevel());

	EXPECT_EQ(first->CurrentLevel().GetEnemies().size(), 10u);
	EXPECT_EQ(second->CurrentLevel().GetEnemies().size(), 15u);
}

TEST(World, AMissingLevelIsAnError) {
	auto loader = SceneLoader::Open("/nonexistent/");
	ASSERT_FALSE(loader);
	EXPECT_NE(loader.error().find("config.json"), std::string::npos);
}

}  // namespace
}  // namespace wolfenstein
