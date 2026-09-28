// Where a shot lands shows: blood on the enemy it hits, a puff of dust and
// a mark on the wall it strikes; and each shot kicks the view up a moment

#include "Core/scene.h"
#include "Profiler/profiler.h"
#include "ShootingManager/shooting_manager.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kEast = 0.0;
constexpr double kSouth = std::numbers::pi / 2;	 // towards +y
constexpr double kWest = std::numbers::pi;
constexpr std::size_t kPistol = 0, kShotgun = 2;

// A corridor along y (a map file's rows run along x), the player at
// (1.5, 1.5); a door at (1, 4) across it
class ImpactTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	ImpactTest()
		: map_(testing::WriteMapFile("wolfenstein_impact_test.txt",
									 {"33333333", "3000D003", "33333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::GameData().weapons, kPistol,
				  testing::TestTextures(), testing::TestSound()) {
		scene_.SetPlayer(player_);
	}

	void Shoot(std::size_t weapon, double theta, double pitch = 0.0) {
		ResolvePlayerShot(scene_, player_.GetWeapon(weapon),
						  Position2D({1.5, 1.5}, theta), pitch);
	}

	std::vector<const IGameObject*> Puffs() const {
		std::vector<const IGameObject*> puffs;
		for (const IGameObject* object : scene_.GetObjects()) {
			if (object->GetObjectType() == ObjectType::EFFECT &&
				object->IsVisible()) {
				puffs.push_back(object);
			}
		}
		return puffs;
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kEast), 2.0, 0.4, 0.4, 1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(ImpactTest, AShotAtAWallMarksTheFaceItStruck) {
	scene_.FinishLoading();
	Shoot(kPistol, kEast);
	const auto marks = scene_.GetWallMarks();
	ASSERT_EQ(marks.size(), 1u);
	EXPECT_EQ(marks[0].x, 2);
	EXPECT_EQ(marks[0].y, 1);
	EXPECT_EQ(marks[0].face, 0) << "its west face, which the shot came at";
	EXPECT_NEAR(marks[0].across, 0.5, 1e-6);
	EXPECT_NEAR(marks[0].down, 0.5, 0.06) << "half a wall up, where shots fly";

	Shoot(kPistol, kWest);
	ASSERT_EQ(scene_.GetWallMarks().size(), 2u);
	EXPECT_EQ(scene_.GetWallMarks()[1].x, 0);
	EXPECT_EQ(scene_.GetWallMarks()[1].face, 1) << "its east face";
}

TEST_F(ImpactTest, AShotAtAWallRaisesDust) {
	scene_.FinishLoading();
	Shoot(kPistol, kEast);
	const auto puffs = Puffs();
	ASSERT_EQ(puffs.size(), 1u);
	EXPECT_LT(puffs[0]->GetPose().x, 2.0) << "on this side of the wall";
	EXPECT_GT(puffs[0]->GetPose().x, 1.9);
}

TEST_F(ImpactTest, AShotAtAnEnemyDrawsBloodNotAMark) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({1.5, 3.5}, -kSouth)));
	scene_.FinishLoading();
	Shoot(kPistol, kSouth);
	EXPECT_TRUE(scene_.GetWallMarks().empty());
	const auto puffs = Puffs();
	ASSERT_EQ(puffs.size(), 1u);
	// In front of the enemy, as the shooter sees it
	EXPECT_GT(puffs[0]->GetPose().y, 1.5);
	EXPECT_LT(puffs[0]->GetPose().y, 3.5);
}

// The mouse looks up and down, as far as the limit either way
TEST_F(ImpactTest, LookingUpAndDownStopsAtTheLimit) {
	player_.SetCommand({.look_up = 0.25});
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetPitch(), 0.25);
	player_.SetCommand({.look_up = 0.25});
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetPitch(), Player::kMaxPitch);
	player_.SetCommand({.look_up = -2.0});
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetPitch(), -Player::kMaxPitch);
	// Applied once, like turning with the mouse
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetPitch(), -Player::kMaxPitch);
}

// Aimed up, a shot marks the wall higher, and the dust flies there
TEST_F(ImpactTest, AShotAimedUpStrikesHigher) {
	scene_.FinishLoading();
	constexpr double kPitch = 0.6;	// the wall is half a unit off: 0.3 higher
	Shoot(kPistol, kEast, kPitch);
	ASSERT_EQ(scene_.GetWallMarks().size(), 1u);
	EXPECT_NEAR(scene_.GetWallMarks()[0].down, 0.2, 0.06) << "0.2 from the top";
	const auto puffs = Puffs();
	ASSERT_EQ(puffs.size(), 1u);
	EXPECT_NEAR(puffs[0]->GetElevation(), 0.3, 1e-6);
}

// Aimed at the floor well short of the wall, a shot marks nothing
TEST_F(ImpactTest, AShotIntoTheFloorMarksNoWall) {
	scene_.FinishLoading();
	Shoot(kPistol, kSouth, -Player::kMaxPitch);	 // the door is 3 units off
	EXPECT_TRUE(scene_.GetWallMarks().empty());
	EXPECT_TRUE(Puffs().empty());
}

// A shotgun's pellets fan out: a spread of marks, one per pellet
TEST_F(ImpactTest, AShotgunBlastFansItsPellets) {
	scene_.FinishLoading();
	Shoot(kShotgun, kEast);
	const auto pellets = testing::Weapon("shotgun").pellets;
	ASSERT_GT(pellets, 1u);
	const auto marks = scene_.GetWallMarks();
	ASSERT_EQ(marks.size(), pellets);
	const auto [low, high] = std::ranges::minmax_element(
		marks, {}, [](const Scene::WallMark& mark) { return mark.across; });
	EXPECT_LT(low->across, 0.5F);
	EXPECT_GT(high->across, 0.5F) << "either side of where it was aimed";
}

// Doors slide away, so they keep no marks
TEST_F(ImpactTest, ADoorKeepsNoMark) {
	scene_.FinishLoading();
	Shoot(kPistol, kSouth);
	EXPECT_TRUE(scene_.GetWallMarks().empty());
	EXPECT_EQ(Puffs().size(), 1u) << "though the dust still flies";
}

TEST_F(ImpactTest, TheWallsKeepTheNewestMarks) {
	scene_.FinishLoading();
	for (std::size_t shot = 0; shot < Scene::kWallMarks + 8; ++shot) {
		Shoot(kPistol, kEast);
	}
	EXPECT_EQ(scene_.GetWallMarks().size(), Scene::kWallMarks);
	EXPECT_EQ(Puffs().size(), Scene::kEffects) << "the oldest puffs give way";
}

TEST_F(ImpactTest, PuffsFadeAway) {
	scene_.FinishLoading();
	Shoot(kPistol, kEast);
	ASSERT_FALSE(Puffs().empty());
	for (int tick = 0; tick < 60; ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_TRUE(Puffs().empty());
	EXPECT_EQ(scene_.GetWallMarks().size(), 1u) << "the mark stays";
}

TEST_F(ImpactTest, AShotKicksTheViewAndItSettles) {
	scene_.FinishLoading();
	player_.SetCommand({.fire = true});
	scene_.Update(kTick);
	const double kick = testing::Weapon("pistol").kick;
	ASSERT_GT(kick, 0.0);
	EXPECT_GT(player_.GetKick(), kick / 2);
	player_.SetCommand({});
	for (int tick = 0; tick < 12; ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_LT(player_.GetKick(), kick / 20) << "settled in a fifth of a second";
}

// Killed, the player falls: the eye drops from half a wall to the floor in
// kFallSeconds, slowly at first, as things fall
TEST_F(ImpactTest, AKilledPlayerFallsToTheFloor) {
	scene_.FinishLoading();
	EXPECT_EQ(player_.GetDeathFall(), 0.0) << "alive";
	EXPECT_DOUBLE_EQ(player_.GetEyeHeight(), 0.5);
	player_.DecreaseHealth(1000.0);
	ASSERT_FALSE(player_.IsAlive());
	EXPECT_EQ(player_.GetDeathFall(), 0.0) << "just killed";

	const int ticks = static_cast<int>(Player::kFallSeconds / kTick);
	double eye = player_.GetEyeHeight();
	for (int tick = 0; tick < ticks / 2; ++tick) {
		scene_.Update(kTick);
		EXPECT_LE(player_.GetEyeHeight(), eye) << "never back up";
		eye = player_.GetEyeHeight();
	}
	EXPECT_LT(player_.GetDeathFall(), 0.5) << "slower at first";
	for (int tick = 0; tick < ticks; ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_DOUBLE_EQ(player_.GetDeathFall(), 1.0);
	EXPECT_LT(player_.GetEyeHeight(), 0.1) << "on the floor";
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
TEST_F(ImpactTest, ShootingAllocatesNothing) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({1.5, 3.5}, -kSouth)));
	scene_.FinishLoading();
	const auto before = AllocationStats::count;
	for (int shot = 0; shot < 100; ++shot) {
		Shoot(kPistol, shot % 2 == 0 ? kSouth : kWest);
		scene_.Update(kTick);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace wolfenstein
