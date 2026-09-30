// Enemies hunting the player find their way past what stands in it: lamps
// (solid, so the route keeps the enemy's body clear of them), and other
// enemies (not solid to each other, so a crowd in a doorway slows the route
// but never shuts it)

#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;
// A map file's rows run along x. The player's corridor and the enemy's,
// two cells wide each, joined by a doorway a cell wide at (3, 7): the
// player at (1.5, 1.5) cannot be seen from the enemy's corridor.
constexpr std::initializer_list<const char*> kCorridors = {
	"3333333333",  //
	"3000000003",  // the player's corridor
	"3000000003",  //
	"3333333033",  // the doorway
	"3000000003",  // the enemy's corridor
	"3000000003",  //
	"3333333333"};

class NavigationTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 3,
											 .dynamic_objects = 1};

	NavigationTest()
		: map_(testing::WriteMapFile("karakale_navigation_test.txt", kCorridors)
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_) {
		scene_.SetPlayer(player_);
	}

	Enemy& AddSoldier(vector2d where) {
		EXPECT_TRUE(
			scene_.AddEnemy(testing::Enemy("soldier"), Position2D(where, 0.0)));
		return *scene_.GetEnemies().back();
	}
	void AddLamp(vector2d where) {
		const DynamicObjectStats& light = testing::GameData().light;
		ASSERT_TRUE(scene_.AddDynamicObject(
			where,
			LoopedAnimation(testing::TestTextures(), "green_light",
							light.animation_speed),
			light.width, light.height, light.radius));
	}
	// Runs until `reached` holds or `seconds` pass; whether it held
	template <typename Reached>
	bool RunUntil(double seconds, Reached reached) {
		for (double t = 0.0; t < seconds; t += kTick) {
			scene_.Update(kTick);
			if (reached()) {
				return true;
			}
		}
		return false;
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, std::numbers::pi / 2), 2.0,
							0.4, 0.4, 1.0};
	Player player_{config_, testing::GameData().weapons, 0,
				   testing::TestTextures(), testing::TestSound()};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
};

// A lamp in the enemy's way, and where the enemy starts
struct LampCase
{
	vector2d lamp;
	vector2d start;
};

// Hunting the player, the enemy gets through the doorway into the player's
// corridor, past the lamp, wherever the lamp stands in the route's cells
class LampTest : public NavigationTest,
				 public ::testing::WithParamInterface<LampCase>
{};

TEST_P(LampTest, AHuntingEnemyGetsPastALamp) {
	const auto [lamp, start] = GetParam();
	Enemy& enemy = AddSoldier(start);
	AddLamp(lamp);
	scene_.FinishLoading();
	// About 9 cells to the doorway at 0.8 a second: 11 s, with room to
	// spare; hunting all the while
	EXPECT_TRUE(RunUntil(16.0,
						 [&] {
							 enemy.Alert();
							 return enemy.GetPose().x < 3.0;
						 }))
		<< "stuck at " << enemy.GetPose().x << ", " << enemy.GetPose().y;
}

INSTANTIATE_TEST_SUITE_P(
	Corridor, LampTest,
	::testing::Values(LampCase{{4.5, 3.5}, {4.5, 1.5}},	 // on one side
					  LampCase{{5.5, 3.5}, {5.5, 1.5}},	 // on the other
					  // Before the doorway, the enemy straight behind it
					  LampCase{{4.5, 7.5}, {5.5, 7.5}}));

// Every cell the route takes keeps the enemy's body clear of the lamp
TEST_F(NavigationTest, TheRouteKeepsClearOfALamp) {
	Enemy& enemy = AddSoldier({4.5, 1.5});
	AddLamp({4.5, 3.5});
	scene_.FinishLoading();
	(void)scene_.GetNavigation().FindPathToPlayer(enemy.GetPosition(),
												  enemy.GetId());
	const auto path = scene_.GetNavigation().GetPath(enemy.GetId());
	ASSERT_FALSE(path.empty());
	const double reach = enemy.GetRadius() + testing::GameData().light.radius;
	for (const GridCell cell : path) {
		EXPECT_GT(NavigationManager::CellCentre(cell).Distance({4.5, 3.5}),
				  reach)
			<< cell.x << ", " << cell.y;
	}
}

// Asked again from where it stands, an enemy is sent the same way: the
// cell it is stepping into does not stand in its own way
TEST_F(NavigationTest, AnEnemyIsNotInItsOwnWay) {
	Enemy& enemy = AddSoldier({4.5, 1.5});
	scene_.FinishLoading();
	NavigationManager& navigation = scene_.GetNavigation();
	(void)navigation.FindPathToPlayer(enemy.GetPosition(), enemy.GetId());
	ASSERT_FALSE(navigation.GetPath(enemy.GetId()).empty());
	const GridCell next = navigation.GetPath(enemy.GetId()).front();
	for (int again = 0; again < 3; ++again) {
		(void)navigation.FindPathToPlayer(enemy.GetPosition(), enemy.GetId());
		ASSERT_FALSE(navigation.GetPath(enemy.GetId()).empty());
		EXPECT_EQ(navigation.GetPath(enemy.GetId()).front(), next) << again;
	}
}

// Two soldiers stand in the doorway, filling it: the one hunting the
// player goes through them, as enemies pass each other
TEST_F(NavigationTest, ACrowdedDoorwayDoesNotStopAHunter) {
	Enemy& hunter = AddSoldier({4.5, 5.5});
	AddSoldier({3.25, 7.25});
	AddSoldier({3.75, 7.75});
	scene_.FinishLoading();
	hunter.Alert();
	EXPECT_TRUE(RunUntil(6.0, [&] { return hunter.GetPose().x < 3.0; }))
		<< "stuck at " << hunter.GetPose().x << ", " << hunter.GetPose().y;
}

}  // namespace
}  // namespace karakale
