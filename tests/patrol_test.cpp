// A patroller walks about near its post, spot to spot, facing the way it
// goes, so a player sees it from the side it passes on; a guard stands its
// ground, looking this way and that. Either notices a player in view, and
// hunts; a patroller that gives up the hunt walks about again.

#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>
#include <set>
#include <utility>

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;
// A sealed pocket beside the hall: a player there is out of every sight
const vector2d kPocket{8.5, 13.5};
// The enemy's post, in the middle of the hall
const vector2d kPost{4.5, 7.5};

// A hall 6 cells wide and 13 long, rows along x, and the pocket
class PatrolTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	PatrolTest()
		: map_(testing::WriteMapFile(
				   "karakale_patrol_test.txt",
				   {"333333333333333", "300000000000003", "300000000000003",
					"300000000000003", "300000000000003", "300000000000003",
					"300000000000003", "333333333333333", "333333333333303",
					"333333333333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_) {}

	// The player at `where`; a soldier at its post, looking along +y,
	// wandering `radius` about it (0: a guard)
	Enemy& Start(vector2d where, double radius) {
		config_.initial_position = Position2D(where, 0.0);
		player_ = std::make_unique<Player>(config_, testing::GameData().weapons,
										   0, testing::TestTextures(),
										   testing::TestSound());
		scene_.SetPlayer(*player_);
		EXPECT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
									Position2D(kPost, std::numbers::pi / 2)));
		scene_.FinishLoading();
		Enemy& enemy = *scene_.GetEnemies().front();
		enemy.SetPatrolRadius(radius);
		return enemy;
	}
	void Run(double seconds) {
		for (double t = 0.0; t < seconds; t += kTick) {
			scene_.Update(kTick);
		}
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, 0.0), 2.0, 0.4, 0.4, 1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	std::unique_ptr<Player> player_;
};

// Unaware of the player, it walks about, near all the time, on to one spot
// after another, never out of its radius of its post
TEST_F(PatrolTest, ItWalksAboutNearItsPost) {
	constexpr double kRadius = 4.0;
	Enemy& enemy = Start(kPocket, kRadius);
	int moving = 0;
	int ticks = 0;
	std::set<std::pair<int, int>> cells;
	for (double t = 0.0; t < 40.0; t += kTick, ++ticks) {
		scene_.Update(kTick);
		ASSERT_EQ(enemy.GetStateType(), EnemyStateType::Patrol) << t;
		EXPECT_LE(enemy.GetPose().Distance(kPost), kRadius + 0.5) << t;
		moving += enemy.IsMoving() ? 1 : 0;
		cells.insert({static_cast<int>(enemy.GetPose().x),
					  static_cast<int>(enemy.GetPose().y)});
	}
	EXPECT_GT(moving, ticks * 8 / 10) << "it keeps walking";
	EXPECT_GE(cells.size(), 8u) << "all about its post, not to and fro";
}

// It strolls: slower than it hunts
TEST_F(PatrolTest, ItStrollsSlowerThanItHunts) {
	Enemy& enemy = Start(kPocket, 4.0);
	double walked = 0.0;
	for (double t = 0.0; t < 5.0; t += kTick) {
		const vector2d before = enemy.GetPose();
		scene_.Update(kTick);
		walked += enemy.GetPose().Distance(before);
	}
	const double full = testing::Enemy("soldier").translation_speed * 5.0;
	EXPECT_LT(walked, 0.75 * full);
	EXPECT_GT(walked, 0.25 * full);
}

// It faces the way it walks: from wherever a player stands, it is seen
// from that side
TEST_F(PatrolTest, ItFacesTheWayItWalks) {
	Enemy& enemy = Start(kPocket, 4.0);
	int checked = 0;
	for (double t = 0.0; t < 10.0; t += kTick) {
		const vector2d before = enemy.GetPose();
		scene_.Update(kTick);
		const vector2d moved = enemy.GetPose() - before;
		if (moved.Magnitude() < 1e-6) {
			continue;
		}
		++checked;
		const double theta = enemy.GetPosition().theta;
		EXPECT_NEAR(std::cos(theta), moved.x / moved.Magnitude(), 0.2) << t;
		EXPECT_NEAR(std::sin(theta), moved.y / moved.Magnitude(), 0.2) << t;
		// Seen from ahead of it, its front; from behind, its back
		const vector2d ahead{std::cos(theta), std::sin(theta)};
		EXPECT_EQ(enemy.ViewFrom(enemy.GetPose() + ahead * 3.0), 0u) << t;
		EXPECT_EQ(enemy.ViewFrom(enemy.GetPose() - ahead * 3.0), 4u) << t;
	}
	EXPECT_GT(checked, 100);
}

// A guard stands its ground, and looks this way and that
TEST_F(PatrolTest, AGuardStandsAndLooksAbout) {
	Enemy& guard = Start(kPocket, 0.0);
	std::set<long> facings;
	for (double t = 0.0; t < 10.0; t += kTick) {
		scene_.Update(kTick);
		ASSERT_EQ(guard.GetStateType(), EnemyStateType::Idle) << t;
		EXPECT_LT(guard.GetPose().Distance(kPost), 1e-9) << t;
		facings.insert(std::lround(guard.GetPosition().theta * 100.0));
	}
	EXPECT_GE(facings.size(), 3u) << "ahead, and over either shoulder";
}

// A player in view it notices, and hunts: ahead of it, or behind
TEST_F(PatrolTest, ItNoticesAPlayerAhead) {
	Enemy& enemy = Start({4.5, 11.5}, 4.0);
	Run(0.5);
	EXPECT_NE(enemy.GetStateType(), EnemyStateType::Patrol);
	EXPECT_NE(enemy.GetStateType(), EnemyStateType::Idle);
}

TEST_F(PatrolTest, ItNoticesAPlayerBehindIt) {
	Enemy& enemy = Start({4.5, 3.5}, 4.0);
	Run(0.5);
	EXPECT_NE(enemy.GetStateType(), EnemyStateType::Patrol);
	EXPECT_NE(enemy.GetStateType(), EnemyStateType::Idle);
}

// Gunfire it hears sets it hunting; with no way to the player (sealed in
// the pocket) and no sight of them, it lost the trail: it walks about again
// at once, never stopping in between
TEST_F(PatrolTest, WithNoWayToThePlayerItWalksOnAtOnce) {
	Enemy& enemy = Start(kPocket, 4.0);
	Run(0.5);
	ASSERT_TRUE(enemy.IsMoving());
	enemy.Alert();
	bool hunted = false;
	for (int tick = 0; tick < 30; ++tick) {
		scene_.Update(kTick);
		hunted = hunted || enemy.GetStateType() == EnemyStateType::Walk;
		EXPECT_TRUE(enemy.IsMoving()) << tick;
	}
	EXPECT_TRUE(hunted) << "it heard, and set off";
	EXPECT_EQ(enemy.GetStateType(), EnemyStateType::Patrol);
	EXPECT_FALSE(enemy.IsAlerted());
}

// From the start it walks about: it never stands first
TEST_F(PatrolTest, ItWalksFromTheStart) {
	Enemy& enemy = Start(kPocket, 4.0);
	EXPECT_EQ(enemy.GetStateType(), EnemyStateType::Patrol);
	scene_.Update(kTick);
	EXPECT_TRUE(enemy.IsMoving());
}

// Hunting, it stands to shoot a player at its range: it does not step on
// the spot
TEST_F(PatrolTest, StandingItDoesNotStep) {
	Enemy& enemy = Start({4.5, 11.0}, 0.0);
	Run(0.5);
	ASSERT_EQ(enemy.GetStateType(), EnemyStateType::Walk);
	ASSERT_FALSE(enemy.IsMoving()) << "at its range, it stands";
	const int frame = enemy.GetTextureId();
	for (int tick = 0; tick < 20; ++tick) {
		scene_.Update(kTick);
		if (enemy.GetStateType() != EnemyStateType::Walk) {
			break;	// it went on to shoot
		}
		EXPECT_EQ(enemy.GetTextureId(), frame) << tick;
	}
}

}  // namespace
}  // namespace karakale
