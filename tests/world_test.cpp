// A World owns everything the game simulates and nothing in it is global,
// so a test can build one without a window or an audio device and play it
// through level loads.

#include "Core/world.h"
#include "Profiler/profiler.h"
#include "test_services.h"
#include <array>
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

// The difficulty sets how much health enemies start with, in every level
TEST(World, DifficultyScalesEnemyHealth) {
	auto world = MakeWorld();
	for (const auto& [difficulty, health] :
		 {std::pair{"easy", 75.0}, std::pair{"normal", 100.0},
		  std::pair{"hard", 130.0}}) {
		ASSERT_TRUE(world->NewGame("mp5", {}, difficulty));
		EXPECT_DOUBLE_EQ(
			world->CurrentLevel().GetEnemies().front()->GetHealth(), health)
			<< difficulty;
		ASSERT_TRUE(world->NextLevel());
		EXPECT_DOUBLE_EQ(
			world->CurrentLevel().GetEnemies().front()->GetHealth(), health)
			<< difficulty << ", level 2";
	}
	EXPECT_FALSE(world->NewGame("mp5", {}, "impossible"));
}

// A save made as a level starts goes on from that level's beginning, at
// its difficulty, and on through the rest of the campaign
TEST(World, AGameContinuesAtItsSavedLevel) {
	auto world = MakeWorld();
	SavedGame saved{.level = 2, .weapon = 1, .difficulty = 2};
	const auto continued = world->ContinueGame(saved);
	ASSERT_TRUE(continued) << continued.error();
	EXPECT_EQ(world->LevelNumber(), 3u);
	EXPECT_EQ(world->CurrentLevel().GetEnemies().size(),
			  CampaignLevel(2).enemies.size());
	EXPECT_EQ(world->Difficulty().name, "hard");
	EXPECT_DOUBLE_EQ(world->GetPlayer().GetPosition().pose.x,
					 CampaignLevel(2).player.pose.x);
	ASSERT_TRUE(world->NextLevel());
	EXPECT_EQ(world->LevelNumber(), 4u);

	saved.level = 99;
	EXPECT_FALSE(world->ContinueGame(saved));
}

// A save made mid-level puts everything back as it was: where the player
// stood, what it carried, the enemies it killed, the pickups it took, the
// map it explored and the clock
TEST(World, AGameComesBackAsItWasLeft) {
	auto world = MakeWorld();
	ASSERT_TRUE(world->NewGame("shotgun", {}, "easy"));
	ASSERT_TRUE(world->NextLevel());
	Scene& level = world->CurrentLevel();
	Player& player = world->GetPlayer();
	player.Restore(61.0, 1, 30);
	player.SetPosition(Position2D({13.5, 15.5}, 1.25));
	level.GetEnemies()[1]->DecreaseHealth(1000.0);
	level.DecreaseAliveEnemies();
	level.GetPickups()[2]->Take();
	level.Explore(13, 15);
	level.Explore(0, 0);
	level.RestoreSeconds(42.5);
	player.SetKeys(KeyBit(KeyColour::Silver));

	const auto captured = world->Capture();
	ASSERT_TRUE(captured);
	// Through the text it is stored as
	std::array<char, 1024> text{};
	const auto size = captured.value_or(SavedGame{}).Format(text);
	ASSERT_GT(size, 0u);
	const auto saved = SavedGame::Parse(std::string_view(text.data(), size));
	ASSERT_TRUE(saved);

	auto later = MakeWorld();
	const auto continued = later->ContinueGame(saved.value_or(SavedGame{}));
	ASSERT_TRUE(continued) << continued.error();
	Scene& again = later->CurrentLevel();
	const Player& back = later->GetPlayer();
	EXPECT_EQ(later->LevelNumber(), 2u);
	EXPECT_EQ(later->Difficulty().name, "easy");
	EXPECT_EQ(back.GetWeapon().GetWeaponName(), "shotgun");
	EXPECT_DOUBLE_EQ(back.GetHealth(), 61.0);
	EXPECT_EQ(back.GetWeapon().GetAmmo(), 1u);
	EXPECT_EQ(back.GetWeapon().GetReserve(), 30u);
	EXPECT_DOUBLE_EQ(back.GetPosition().pose.x, 13.5);
	EXPECT_DOUBLE_EQ(back.GetPosition().pose.y, 15.5);
	EXPECT_DOUBLE_EQ(back.GetPosition().theta, 1.25);
	EXPECT_FALSE(again.GetEnemies()[1]->IsAlive());
	EXPECT_TRUE(again.GetEnemies()[0]->IsAlive());
	EXPECT_EQ(again.GetNumberOfAliveEnemies(),
			  CampaignLevel(1).enemies.size() - 1);
	EXPECT_TRUE(again.GetPickups()[2]->IsTaken());
	EXPECT_FALSE(again.GetPickups()[0]->IsTaken());
	EXPECT_TRUE(again.IsExplored(13, 15));
	EXPECT_TRUE(again.IsExplored(0, 0));
	EXPECT_FALSE(again.IsExplored(1, 1));
	EXPECT_DOUBLE_EQ(again.GetStats().seconds, 42.5);
	EXPECT_TRUE(back.HasKey(KeyColour::Silver));
	EXPECT_FALSE(back.HasKey(KeyColour::Gold));
}

// Saved only when nothing is fighting the player
TEST(World, ItIsQuietWhenNothingIsFighting) {
	auto world = MakeWorld();
	ASSERT_TRUE(world->NewGame("mp5"));
	for (int tick = 0; tick < 4 * 60; ++tick) {
		world->CurrentLevel().Update(1.0 / 60.0);
	}
	EXPECT_TRUE(world->IsQuiet());
	world->GetPlayer().DecreaseHealth(5.0);
	EXPECT_FALSE(world->IsQuiet()) << "just hurt";
	for (int tick = 0; tick < 4 * 60; ++tick) {
		world->CurrentLevel().Update(1.0 / 60.0);
	}
	EXPECT_TRUE(world->IsQuiet());
	world->CurrentLevel().GetEnemies()[0]->TransitionTo(EnemyStateType::Walk);
	EXPECT_FALSE(world->IsQuiet()) << "an enemy coming";
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
// Games are saved while they run
TEST(World, CapturingAGameAllocatesNothing) {
	auto world = MakeWorld();
	ASSERT_TRUE(world->NewGame("mp5"));
	std::array<char, 1024> text{};
	const auto before = AllocationStats::count;
	for (int i = 0; i < 10; ++i) {
		const auto saved = world->Capture();
		ASSERT_TRUE(saved);
		(void)saved.value_or(SavedGame{}).Format(text);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}

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
