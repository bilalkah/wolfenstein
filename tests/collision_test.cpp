// Bodies do not pass through each other: the player stops against living
// enemies and lamps, enemies against the player and lamps. What can be
// walked over (pickups, the dead) does not stop anyone.

#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

namespace karakale {
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
				   "karakale_collision_test.txt",
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

// Walking at a living enemy, the player never ends a tick inside it: it
// comes up against it and, as the enemy shifts, slides round it
TEST_F(CollisionTest, ThePlayerNeverEntersALivingEnemy) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({3.5, 4.5}, -kFacingDown)));
	scene_.FinishLoading();
	const Enemy& enemy = *scene_.GetEnemies().front();
	const double reach = player_.GetWidth() / 2 + enemy.GetRadius();
	player_.SetCommand(PlayerCommand{.forward = 1});
	double closest = 1e9;
	for (double t = 0.0; t < 3.0; t += kTick) {
		scene_.Update(kTick);
		closest = std::min(closest, DistanceTo(enemy.GetPose()));
	}
	EXPECT_GE(closest, reach - 1e-9);
	EXPECT_LT(closest, reach + 0.05) << "it did come up against it";
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

// Walking into something off centre slides round it and carries on, the
// way a move along a wall does
TEST_F(CollisionTest, AMoveIntoSomethingSlidesRoundIt) {
	AddLamp({3.5, 4.5});
	scene_.FinishLoading();
	player_.SetPosition(Position2D({3.35, 2.5}, kFacingDown));
	Walk(PlayerCommand{.forward = 1}, 2.0);
	EXPECT_GT(player_.GetPose().y, 5.0) << "went round it";
	EXPECT_LT(player_.GetPose().x, 3.35) << "round its near side";
}

// A step into something ends touching it, not short of it: the body goes
// as far as it can
TEST(PushOut, AStepEndsAgainstWhatItMeets) {
	const vector2d centre{0.0, 1.0};
	// Head on: stops at its edge
	const vector2d head_on =
		PushOutOf(centre, 0.2, {0.0, 0.0}, {0.0, 0.9}, 0.2);
	EXPECT_NEAR(head_on.y, 0.6, 1e-9);
	EXPECT_NEAR(head_on.x, 0.0, 1e-9);
	// Glancing: pushed sideways, still moving on
	const vector2d glancing =
		PushOutOf(centre, 0.2, {-0.3, 0.3}, {-0.2, 0.9}, 0.2);
	EXPECT_NEAR(glancing.Distance(centre), 0.4, 1e-9);
	EXPECT_GT(glancing.y, 0.3);
	// Clear of it: untouched
	const vector2d clear = PushOutOf(centre, 0.2, {1.0, 0.0}, {1.0, 0.5}, 0.2);
	EXPECT_DOUBLE_EQ(clear.x, 1.0);
	EXPECT_DOUBLE_EQ(clear.y, 0.5);
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
	const vector2d past = ResolveObjectCollisions(
		objects, first, first->GetPose(), {3.5, 5.3}, first->GetRadius(),
		/*ignore_enemies=*/true);
	EXPECT_DOUBLE_EQ(past.y, 5.3);
	// Into the lamp: stopped at its edge
	const vector2d stopped = ResolveObjectCollisions(
		objects, second, {5.0, 4.5}, {5.2, 4.5}, second->GetRadius(),
		/*ignore_enemies=*/true);
	EXPECT_NEAR(stopped.Distance({5.5, 4.5}),
				second->GetRadius() + testing::GameData().light.radius, 1e-9);
}

// Coming at a wall's corner at an angle, the body stops at the corner as it
// would at the wall's face: no part of it enters the wall
TEST(WallCollision, CornersAreSolidToo) {
	// A room with a pillar at (3, 3); walk diagonally at its corner
	const Map map(
		testing::WriteMapFile("karakale_corner_test.txt",
							  {"3333333", "3000003", "3000003", "3003003",
							   "3000003", "3000003", "3333333"})
			.string());
	const auto inside_a_wall = [&](const vector2d& pose) {
		constexpr double kHalf = kCollisionDistance * 0.98;
		for (const double dx : {-kHalf, kHalf}) {
			for (const double dy : {-kHalf, kHalf}) {
				if (map.IsBlocked(vector2d{pose.x + dx, pose.y + dy})) {
					return true;
				}
			}
		}
		return false;
	};
	for (const double angle : {0.6, 0.785, 1.0}) {
		vector2d pose{1.6, 1.6};
		const vector2d step{0.01 * std::cos(angle), 0.01 * std::sin(angle)};
		for (int tick = 0; tick < 300; ++tick) {
			if (!CheckWallCollision(map, pose, {step.x, 0})) {
				pose.x += step.x;
			}
			if (!CheckWallCollision(map, pose, {0, step.y})) {
				pose.y += step.y;
			}
			ASSERT_FALSE(inside_a_wall(pose))
				<< "angle " << angle << " at " << pose.x << "," << pose.y;
		}
	}
}

// Flush against a wall, a body still slides along it, and can always step
// away from it
TEST(WallCollision, AlongAndAwayFromAWall) {
	const Map map(
		testing::WriteMapFile("karakale_along_test.txt",
							  {"33333", "30003", "30003", "30003", "33333"})
			.string());
	// Pressed against the wall at x = 1 (standing just clear of it)
	const vector2d pose{1.0 + kCollisionDistance + 1e-6, 2.5};
	EXPECT_TRUE(CheckWallCollision(map, pose, {-0.01, 0}));
	EXPECT_FALSE(CheckWallCollision(map, pose, {0, 0.05})) << "along it";
	EXPECT_FALSE(CheckWallCollision(map, pose, {0.05, 0})) << "away from it";
}

// Each body keeps its own size from the walls: a bigger one stops further
// off
TEST(WallCollision, EachBodyKeepsItsOwnSize) {
	const Map map(testing::WriteMapFile("karakale_wall_size_test.txt",
										{"3333", "3003", "3003", "3333"})
					  .string());
	const vector2d pose{1.22, 1.5};	 // 0.22 from the wall's face at x = 1
	EXPECT_FALSE(CheckWallCollision(map, pose, {-0.01, 0.0}, 0.2))
		<< "the player's size still fits";
	EXPECT_TRUE(CheckWallCollision(map, pose, {-0.01, 0.0}, 0.25))
		<< "a demon's is against it already";
}

// Walking forward and sideways at once is no faster than either
TEST_F(CollisionTest, ADiagonalIsNoFaster) {
	const vector2d start = player_.GetPose();
	Walk({.forward = 1, .strafe = 1}, 0.5);
	const double diagonal = DistanceTo(start);
	player_.SetPosition(Position2D(start, kFacingDown));
	Walk({.forward = 1}, 0.5);
	EXPECT_NEAR(diagonal, DistanceTo(start), 1e-9);
}

}  // namespace
}  // namespace karakale
