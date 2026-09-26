// A World owns everything the game simulates and nothing in it is global,
// so a test can build one without a window or an audio device and play it
// through level loads.

#include "Core/world.h"
#include "Profiler/profiler.h"
#include "test_services.h"
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace wolfenstein {
namespace {

std::unique_ptr<World> MakeWorld() {
	auto loader = SceneLoader::Open(RESOURCE_DIR);
	EXPECT_TRUE(loader) << loader.error();
	return std::make_unique<World>(testing::TestTextures(), std::move(*loader),
								   std::make_unique<SoundManager>());
}

// What the campaign's level `index` (from 0) declares, read from its file
LevelData CampaignLevel(std::size_t index) {
	std::ifstream file(std::string(RESOURCE_DIR) + "levels/" +
					   testing::GameData().levels.at(index));
	auto level = ParseLevel(file);
	EXPECT_TRUE(level) << level.error();
	return std::move(*level);
}

TEST(World, NewGameLoadsTheFirstLevelWithThePlayerInIt) {
	auto world = MakeWorld();
	ASSERT_FALSE(world->HasLevel());

	const auto started = world->NewGame("mp5");
	ASSERT_TRUE(started) << started.error();
	const LevelData expected = CampaignLevel(0);
	Scene& level = world->CurrentLevel();
	EXPECT_EQ(level.GetEnemies().size(), expected.enemies.size());
	EXPECT_EQ(&level.GetPlayer(), &world->GetPlayer());
	EXPECT_DOUBLE_EQ(world->GetPlayer().GetPosition().pose.x,
					 expected.player.pose.x);
	EXPECT_EQ(world->LevelNumber(), 1u);
	EXPECT_EQ(world->LevelName(), expected.name);
	EXPECT_TRUE(world->HasNextLevel());
}

// The path that swaps levels while the game runs: each next level replaces
// the finished one and the player moves into it, through the whole campaign
TEST(World, NextLevelPlaysTheCampaignInOrder) {
	auto world = MakeWorld();
	ASSERT_TRUE(world->NewGame("shotgun"));
	const std::size_t levels = testing::GameData().levels.size();
	for (std::size_t index = 1; index < levels; ++index) {
		ASSERT_TRUE(world->HasNextLevel());
		const auto next = world->NextLevel();
		ASSERT_TRUE(next) << next.error();
		EXPECT_EQ(world->LevelNumber(), index + 1);
		EXPECT_EQ(world->CurrentLevel().GetEnemies().size(),
				  CampaignLevel(index).enemies.size());
		EXPECT_EQ(&world->CurrentLevel().GetPlayer(), &world->GetPlayer());
	}
	EXPECT_FALSE(world->HasNextLevel());  // the last level ends the campaign
}

// A new game replaces both the level and the player it borrows
TEST(World, NewGameStartsOver) {
	auto world = MakeWorld();
	ASSERT_TRUE(world->NewGame("mp5"));
	ASSERT_TRUE(world->NextLevel());

	ASSERT_TRUE(world->NewGame("mp5"));
	EXPECT_EQ(world->LevelNumber(), 1u);
	EXPECT_EQ(world->CurrentLevel().GetEnemies().size(),
			  CampaignLevel(0).enemies.size());
	EXPECT_TRUE(world->HasNextLevel());
}

// Nothing is shared between worlds: two can run side by side (a server and
// a client, later)
TEST(World, WorldsAreIndependent) {
	auto first = MakeWorld();
	auto second = MakeWorld();
	ASSERT_TRUE(first->NewGame("mp5"));
	ASSERT_TRUE(second->NewGame("mp5"));
	ASSERT_TRUE(second->NextLevel());

	EXPECT_EQ(first->CurrentLevel().GetEnemies().size(),
			  CampaignLevel(0).enemies.size());
	EXPECT_EQ(second->CurrentLevel().GetEnemies().size(),
			  CampaignLevel(1).enemies.size());
}

// The benchmark plays its own level, outside the campaign
TEST(World, TheBenchmarkLevelIsNotPartOfTheCampaign) {
	auto world = MakeWorld();
	ASSERT_TRUE(world->NewGame("mp5", world->Config().benchmark_level));
	EXPECT_FALSE(world->HasNextLevel());
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
// Everything a game needs is set up when the World is created (levels read,
// arena sized for the largest level, room for the player and the scene):
// after that, starting games and changing levels never allocates
TEST(World, PlayingAllocatesNothingAfterStartup) {
	auto world = MakeWorld();
	const auto before = AllocationStats::count;
	ASSERT_TRUE(world->NewGame("mp5"));
	while (world->HasNextLevel()) {
		ASSERT_TRUE(world->NextLevel());
	}
	ASSERT_TRUE(world->NewGame("shotgun"));
	ASSERT_TRUE(world->NewGame("mp5", world->Config().benchmark_level));
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

TEST(World, AMissingLevelIsAnError) {
	auto loader = SceneLoader::Open("/nonexistent/");
	ASSERT_FALSE(loader);
	EXPECT_NE(loader.error().find("config.json"), std::string::npos);
}

}  // namespace
}  // namespace wolfenstein
