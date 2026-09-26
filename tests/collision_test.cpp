// Bodies do not pass through each other: the player stops against living
// enemies and lamps, enemies against the player and lamps. What can be
// walked over (pickups, the dead) does not stop anyone.

#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// An open room, the player at (3.5, 1.5) facing down it (+y), and
// something to walk into at (3.5, 4.5)
class CollisionTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 2,
											 .dynamic_objects = 1};

	CollisionTest()
		: map_(testing::WriteMapFile(
				   "wolfenstein_collision_test.txt",
				   {"33333333", "30000003", "30000003", "30000003", "30000003",
					"30000003", "33333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound()) {
		scene_.SetPlayer(player_);
	}

	void AddLamp(vector2d where) {
		const DynamicObjectStats& light = testing::GameData().light;
		ASSERT_TRUE(scene_.AddDynamicObject(
			where,
			LoopedAnimation(testing::TestTextures(), "green_light",
							light.animation_speed),
			light.width, light.height, light.radius));
	}
	void Walk(PlayerCommand command, double seconds) {
		player_.SetCommand(command);
		for (double t = 0.0; t < seconds; t += kTick) {
			scene_.Update(kTick);
		}
	}
	double DistanceTo(vector2d where) const {
		return player_.GetPose().Distance(where);
	}

	CharacterConfig config_{Position2D({3.5, 1.5}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(CollisionTest, ThePlayerStopsAgainstALivingEnemy) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({3.5, 4.5}, -kFacingDown)));
	scene_.FinishLoading();
	Enemy& enemy = *scene_.GetEnemies().front();
	Walk(PlayerCommand{.forward = 1}, 3.0);
	const double reach = player_.GetWidth() / 2 + enemy.GetWidth() / 2;
	EXPECT_GE(DistanceTo(enemy.GetPose()), reach - 1e-9);
	EXPECT_LT(DistanceTo(enemy.GetPose()), reach + 0.1) << "right up to it";
}

TEST_F(CollisionTest, TheDeadCanBeWalkedOver) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({3.5, 4.5}, -kFacingDown)));
	scene_.FinishLoading();
	scene_.GetEnemies().front()->RestoreDead();
	scene_.DecreaseAliveEnemies();
	Walk(PlayerCommand{.forward = 1}, 2.0);
	EXPECT_GT(player_.GetPose().y, 5.0) << "past where it lies";
}

TEST_F(CollisionTest, ThePlayerStopsAgainstALamp) {
	AddLamp({3.5, 4.5});
	scene_.FinishLoading();
	Walk(PlayerCommand{.forward = 1}, 3.0);
	const double reach =
		player_.GetWidth() / 2 + testing::GameData().light.radius;
	EXPECT_GE(DistanceTo({3.5, 4.5}), reach - 1e-9);
	EXPECT_LT(player_.GetPose().y, 4.5);
}

// Blocked on one axis, the player keeps moving on the other: it slides
// along what it walks into instead of sticking to it
TEST_F(CollisionTest, ABlockedMoveSlidesAlong) {
	AddLamp({3.5, 4.5});
	scene_.FinishLoading();
	player_.SetPosition(Position2D({3.4, 3.5}, kFacingDown + 0.6));
	Walk(PlayerCommand{.forward = 1}, 1.5);
	EXPECT_GT(player_.GetPose().y, 4.5) << "went round it";
}

// Starting inside something (a bad spawn, a restored game), stepping away
// is always allowed
TEST_F(CollisionTest, AnOverlapCanBeLeft) {
	AddLamp({3.5, 1.6});
	scene_.FinishLoading();
	Walk(PlayerCommand{.forward = -1}, 0.5);
	EXPECT_LT(player_.GetPose().y, 1.4);
}

TEST_F(CollisionTest, EnemiesPassEachOtherButNotLamps) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({3.5, 4.5}, 0.0)));
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({3.5, 5.5}, 0.0)));
	AddLamp({5.5, 4.5});
	const Enemy* first = scene_.GetEnemies()[0];
	const Enemy* second = scene_.GetEnemies()[1];
	const auto objects = scene_.GetObjects();
	// Into the other enemy: allowed, enemies do not block each other
	EXPECT_FALSE(CheckObjectCollision(objects, first, first->GetPose(),
									  {3.5, 5.3}, first->GetWidth() / 2,
									  /*ignore_enemies=*/true));
	// Into the lamp: blocked
	EXPECT_TRUE(CheckObjectCollision(objects, second, {5.0, 4.5}, {5.2, 4.5},
									 second->GetWidth() / 2,
									 /*ignore_enemies=*/true));
}

// The route to the player goes round a lamp in the way
TEST_F(CollisionTest, EnemiesRouteAroundLamps) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({3.5, 5.5}, -kFacingDown)));
	AddLamp({3.5, 3.5});
	scene_.FinishLoading();
	const Enemy& enemy = *scene_.GetEnemies().front();
	(void)scene_.GetNavigation().FindPathToPlayer(enemy.GetPosition(),
												  enemy.GetId());
	const auto path = scene_.GetNavigation().GetPath(enemy.GetId());
	ASSERT_FALSE(path.empty());
	const auto lamp_cell = static_cast<int>(3.5 / NavigationManager::kCellSize);
	const GridCell lamp{lamp_cell, lamp_cell};
	EXPECT_TRUE(std::ranges::none_of(path, [&](const GridCell& cell) {
		return cell.x == lamp.x && cell.y == lamp.y;
	}));
}

}  // namespace
}  // namespace wolfenstein
