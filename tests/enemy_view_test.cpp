// An enemy is seen from the side the viewer stands on: its front, its back,
// its left or right, and the diagonals between; and it turns to face where
// it walks, and the player it shoots at. Dead, it stays lying where it fell
// as the viewer walks round it.

#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;

// An open room, the player at (1.5, 1.5), one soldier facing +x
class EnemyViewTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	EnemyViewTest()
		: map_(testing::WriteMapFile(
				   "wolfenstein_enemy_view_test.txt",
				   {"3333333333", "3000000003", "3000000003", "3000000003",
					"3000000003", "3000000003", "3000000003", "3000000003",
					"3000000003", "3333333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_) {
		scene_.SetPlayer(player_);
	}
	Enemy& AddSoldier(vector2d where, double theta = 0.0) {
		EXPECT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
									Position2D(where, theta)));
		scene_.FinishLoading();
		return *scene_.GetEnemies().front();
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, 0.0), 2.0, 0.4, 0.4, 1.0};
	Player player_{config_, testing::GameData().weapons, 0,
				   testing::TestTextures(), testing::TestSound()};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
};

// Doom's rotations less one: 0 its front, 4 its back; facing the viewer's
// right (+x, seen from +y, where +x is the viewer's right) shows its left
// side, 6
TEST_F(EnemyViewTest, TheSideTheViewerStandsOn) {
	const Enemy& enemy = AddSoldier({4.5, 4.5});
	const vector2d at = enemy.GetPose();
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{3.0, 0.0}), 0u) << "in front";
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{-3.0, 0.0}), 4u) << "behind";
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{0.0, 3.0}), 6u);
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{0.0, -3.0}), 2u);
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{3.0, 3.0}), 7u) << "front, +y side";
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{3.0, -3.0}), 1u);
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{-3.0, -3.0}), 3u);
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{-3.0, 3.0}), 5u);
	// A little off straight ahead is still its front
	EXPECT_EQ(enemy.ViewFrom(at + vector2d{3.0, 0.9}), 0u);
}

// Hunting the player from across the room, out of its weapon's reach, it
// walks, facing the way it goes
TEST_F(EnemyViewTest, ItFacesTheWayItWalks) {
	Enemy& enemy = AddSoldier({8.5, 8.5});
	enemy.Alert();
	int moving = 0;
	for (int tick = 0; tick < 30; ++tick) {
		const vector2d before = enemy.GetPose();
		scene_.Update(kTick);
		const vector2d moved = enemy.GetPose() - before;
		if (moved.Magnitude() > 1e-6) {
			++moving;
			const double theta = enemy.GetPosition().theta;
			EXPECT_NEAR(std::cos(theta), moved.x / moved.Magnitude(), 0.2);
			EXPECT_NEAR(std::sin(theta), moved.y / moved.Magnitude(), 0.2);
		}
	}
	EXPECT_GT(moving, 20) << "it walked";
}

// Shooting, it faces the player: seen from where the player stands, its
// front. It stands looking along -x, the player off to one side ahead.
TEST_F(EnemyViewTest, ItFacesThePlayerItShoots) {
	Enemy& enemy = AddSoldier({4.5, 4.5}, std::numbers::pi);
	ASSERT_NE(enemy.ViewFrom(player_.GetPose()), 0u);
	for (int tick = 0; tick < 300 && player_.GetHealth() == 100.0; ++tick) {
		scene_.Update(kTick);
	}
	ASSERT_LT(player_.GetHealth(), 100.0) << "it never shot";
	EXPECT_EQ(enemy.ViewFrom(player_.GetPose()), 0u);
}

// Dead, it lies across the way it faced the player who killed it: seen
// from that side as drawn, from the far side mirrored, from its head or
// feet narrow
TEST_F(EnemyViewTest, TheDeadStayWhereTheyFell) {
	Enemy& enemy = AddSoldier({4.5, 4.5});
	enemy.RestoreDead();
	ASSERT_EQ(enemy.GetStateType(), EnemyStateType::Death);
	const vector2d at = enemy.GetPose();
	const vector2d towards = player_.GetPose() - at;
	const vector2d killer = towards / towards.Magnitude() * 3.0;
	const vector2d side{-killer.y, killer.x};

	const auto front = enemy.SeenFrom(at + killer);
	EXPECT_FALSE(front.mirrored);
	EXPECT_DOUBLE_EQ(front.width, enemy.GetWidth());

	const auto behind = enemy.SeenFrom(at - killer);
	EXPECT_TRUE(behind.mirrored) << "its head on the other side";
	EXPECT_DOUBLE_EQ(behind.width, enemy.GetWidth());
	EXPECT_EQ(behind.texture_id, front.texture_id);

	const auto end_on = enemy.SeenFrom(at + side);
	EXPECT_LT(end_on.width, 0.5 * enemy.GetWidth()) << "seen along it";
	EXPECT_GT(end_on.width, 0.0);
}

// Alive, it is never mirrored: each side has frames of its own
TEST_F(EnemyViewTest, TheLivingHaveASideForEachView) {
	const Enemy& enemy = AddSoldier({4.5, 4.5});
	const vector2d at = enemy.GetPose();
	for (const vector2d offset : {vector2d{3.0, 0.0}, vector2d{-3.0, 0.0},
								  vector2d{0.0, 3.0}, vector2d{2.0, -2.0}}) {
		const auto seen = enemy.SeenFrom(at + offset);
		EXPECT_FALSE(seen.mirrored);
		EXPECT_DOUBLE_EQ(seen.width, enemy.GetWidth());
	}
}

}  // namespace
}  // namespace wolfenstein
