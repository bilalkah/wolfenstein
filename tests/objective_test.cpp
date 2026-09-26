// A level's exit opens once its objectives are done; using it then ends the
// level. A level without an exit ends when its enemies are all dead.

#include "Core/level_data.h"
#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <sstream>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;

TEST(Exit, AnXIsTheExitSwitch) {
	const Map map(testing::WriteMapFile("wolfenstein_exit_map_test.txt",
										{"3333333", "3000003", "333333X"})
					  .string());
	ASSERT_TRUE(map.HasExit());
	EXPECT_TRUE(map.IsExit(2, 6));
	EXPECT_TRUE(map.IsWall(2, 6));
	EXPECT_EQ((map.GetCells()[2, 6]), Map::kExitWall);

	const auto two = testing::WriteMapFile("wolfenstein_exit_two_test.txt",
										   {"33X3333", "3000003", "333333X"});
	const auto rejected = Map::FromFile(two.string());
	ASSERT_FALSE(rejected);
	EXPECT_NE(rejected.error().find("exit"), std::string::npos);
}

TEST(Exit, LevelsDeclareTheirObjectivesAndTargets) {
	std::istringstream input(R"({
		"map": "m.txt", "player": {"position": {"x": 1, "y": 2, "theta": 0}},
		"enemies": [{"type": "soldier", "target": true,
					 "position": {"x": 3, "y": 4, "theta": 0}},
					{"type": "soldier", "position": {"x": 5, "y": 4, "theta": 0}}],
		"dynamicObjects": [],
		"objectives": [{"type": "kill_targets", "text": "Kill the officer"}]})");
	const auto level = ParseLevel(input);
	ASSERT_TRUE(level) << level.error();
	ASSERT_EQ(level->objectives.size(), 1u);
	EXPECT_EQ(level->objectives[0].type, Objective::Type::KillTargets);
	EXPECT_EQ(level->objectives[0].text, "Kill the officer");
	EXPECT_TRUE(level->enemies[0].target);
	EXPECT_FALSE(level->enemies[1].target);

	std::istringstream unknown(R"({
		"map": "m.txt", "player": {"position": {"x": 1, "y": 2, "theta": 0}},
		"enemies": [], "dynamicObjects": [],
		"objectives": [{"type": "win", "text": "?"}]})");
	const auto rejected = ParseLevel(unknown);
	ASSERT_FALSE(rejected);
	EXPECT_NE(rejected.error().find("win"), std::string::npos);
}

// A corridor ending at the exit switch, the player just before it; two
// soldiers stand in it, the first of them a target
class ExitTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 2};

	explicit ExitTest(const char* last_row = "333X333")
		: map_(testing::WriteMapFile(
				   "wolfenstein_exit_test.txt",
				   {"3333333", "3000003", "3000003", "3000003", last_row})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound()) {
		scene_.SetPlayer(player_);
		for (const double y : {1.5, 5.5}) {
			EXPECT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
										Position2D({1.5, y}, 0.0)));
		}
		scene_.GetEnemies()[0]->SetTarget(true);
	}

	void Kill(std::size_t index) {
		scene_.GetEnemies()[index]->DecreaseHealth(1000.0);
		scene_.DecreaseAliveEnemies();
	}
	void UseAhead() {
		player_.SetCommand(PlayerCommand{.use = true});
		scene_.Update(kTick);
		player_.SetCommand(PlayerCommand{});
	}

	// Facing the switch at (4, 3)
	CharacterConfig config_{Position2D({3.5, 3.5}, 0.0), 2.0, 0.4, 0.4, 1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(ExitTest, TheExitWaitsForTheObjectives) {
	scene_.SetGoals(/*kill_all=*/true, /*kill_targets=*/false);
	scene_.FinishLoading();
	UseAhead();
	EXPECT_FALSE(scene_.IsComplete());
	EXPECT_EQ(scene_.GetNotice(), Scene::Notice::ExitLocked);

	Kill(0);
	Kill(1);
	EXPECT_TRUE(scene_.ObjectivesDone());
	EXPECT_FALSE(scene_.IsComplete()) << "not until the exit is used";
	UseAhead();
	EXPECT_TRUE(scene_.IsComplete());
}

TEST_F(ExitTest, KillingTheTargetsIsEnough) {
	scene_.SetGoals(/*kill_all=*/false, /*kill_targets=*/true);
	scene_.FinishLoading();
	EXPECT_EQ(scene_.TargetsLeft(), 1u);
	Kill(0);
	EXPECT_EQ(scene_.TargetsLeft(), 0u);
	EXPECT_TRUE(scene_.ObjectivesDone());
	UseAhead();
	EXPECT_TRUE(scene_.IsComplete());
	EXPECT_EQ(scene_.GetNumberOfAliveEnemies(), 1u);
}

class NoExitTest : public ExitTest
{
  protected:
	NoExitTest() : ExitTest("3333333") {}
};

// Without an exit (the benchmark's level) a level ends with its last enemy
TEST_F(NoExitTest, ALevelWithoutAnExitEndsWhenCleared) {
	scene_.FinishLoading();
	Kill(0);
	EXPECT_FALSE(scene_.IsComplete());
	Kill(1);
	EXPECT_TRUE(scene_.IsComplete());
}

}  // namespace
}  // namespace wolfenstein
