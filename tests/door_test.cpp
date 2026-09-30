// Doors: a sliding plane across a cell between two walls. Closed, it stops
// movement, sight and shots; the player opens one by using it, an enemy by
// walking up to it, and it closes again once its doorway is clear.

#include "Camera/raycaster.h"
#include "Camera/single_raycaster.h"
#include "Core/scene.h"
#include "Profiler/profiler.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>
#include <string>

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// A corridor along y with a door at (1, 3): walls above and below it, so
// the door is the plane y = 3.5. `door` is D, or G or S for a locked one.
Map CorridorWithDoor(const char* name, char door = 'D') {
	std::string row = "300D003";
	row[3] = door;
	return Map(testing::WriteMapFile(name, {"3333333", row.c_str(), "3333333"})
				   .string());
}

TEST(Door, AMapReadsItsDoors) {
	Map map = CorridorWithDoor("karakale_door_map_test.txt");
	ASSERT_EQ(map.GetDoors().size(), 1u);
	const Door* door = map.FindDoor(1, 3);
	ASSERT_NE(door, nullptr);
	EXPECT_FALSE(door->across_x);
	EXPECT_EQ(map.FindDoor(1, 2), nullptr);

	// Closed it blocks, open it lets through; it is never a wall
	EXPECT_TRUE(map.IsBlocked(1, 3));
	EXPECT_FALSE(map.IsWall(1, 3));
	map.SetDoorOpenness(0, 1.0);
	EXPECT_FALSE(map.IsBlocked(1, 3));
	map.SetDoorOpenness(0, 5.0);
	EXPECT_DOUBLE_EQ(map.GetDoors()[0].openness, 1.0);
}

TEST(Door, GAndSAreLockedDoors) {
	const Map gold = CorridorWithDoor("karakale_door_gold_test.txt", 'G');
	ASSERT_NE(gold.FindDoor(1, 3), nullptr);
	EXPECT_EQ(gold.FindDoor(1, 3)->lock, KeyColour::Gold);
	EXPECT_TRUE(gold.IsLockedDoor(1, 3));
	const Map silver = CorridorWithDoor("karakale_door_silver_test.txt", 'S');
	EXPECT_EQ(silver.FindDoor(1, 3)->lock, KeyColour::Silver);
	const Map plain = CorridorWithDoor("karakale_door_plain_test.txt");
	EXPECT_FALSE(plain.IsLockedDoor(1, 3));
}

TEST(Door, ADoorStandsBetweenTwoWalls) {
	const auto loose = testing::WriteMapFile("karakale_door_loose_test.txt",
											 {"3300033", "300D003", "3333333"});
	const auto map = Map::FromFile(loose.string());
	ASSERT_FALSE(map);
	EXPECT_NE(map.error().find("door"), std::string::npos) << map.error();
}

TEST(Door, AClosedDoorStopsRaysAtItsMiddle) {
	Map map = CorridorWithDoor("karakale_door_ray_test.txt");
	const Ray closed =
		CastRay(map, Position2D({1.5, 1.5}, kFacingDown), kFacingDown, 15.0);
	ASSERT_TRUE(closed.is_hit);
	EXPECT_TRUE(Map::IsDoorCell(static_cast<std::uint16_t>(closed.wall_id)));
	EXPECT_NEAR(closed.distance, 2.0, 1e-9);  // the plane y = 3.5

	map.SetDoorOpenness(0, 1.0);
	const Ray open =
		CastRay(map, Position2D({1.5, 1.5}, kFacingDown), kFacingDown, 15.0);
	ASSERT_TRUE(open.is_hit);
	EXPECT_NEAR(open.distance, 4.5, 1e-9);	// the wall at y = 6
}

// Half open, the door covers the far half of the doorway: rays through the
// near half pass, and the rest show the door's texture from halfway along
TEST(Door, AHalfOpenDoorLetsRaysThroughItsGap) {
	Map map = CorridorWithDoor("karakale_door_half_test.txt");
	map.SetDoorOpenness(0, 0.5);
	const Ray gap =
		CastRay(map, Position2D({1.2, 1.5}, kFacingDown), kFacingDown, 15.0);
	EXPECT_NEAR(gap.distance, 4.5, 1e-9);
	const Ray door =
		CastRay(map, Position2D({1.7, 1.5}, kFacingDown), kFacingDown, 15.0);
	EXPECT_NEAR(door.distance, 2.0, 1e-9);
	EXPECT_DOUBLE_EQ(door.texture_shift, 0.5);
}

TEST(Door, AClosedDoorBlocksSight) {
	Map map = CorridorWithDoor("karakale_door_sight_test.txt");
	EXPECT_FALSE(CastLineOfSight(map, {1.5, 1.5}, {1.5, 5.5}).is_hit);
	map.SetDoorOpenness(0, 1.0);
	EXPECT_TRUE(CastLineOfSight(map, {1.5, 1.5}, {1.5, 5.5}).is_hit);
}

// The player in the corridor, a step before the door (locked when `door` is
// G or S)
class DoorSceneTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	explicit DoorSceneTest(char door = 'D')
		: map_(CorridorWithDoor("karakale_door_scene_test.txt", door)),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound()) {
		scene_.SetPlayer(player_);
	}

	void Run(double seconds) {
		for (double t = 0.0; t < seconds; t += kTick) {
			scene_.Update(kTick);
		}
	}
	double Openness() const { return scene_.GetMap().GetDoors()[0].openness; }

	CharacterConfig config_{Position2D({1.5, 2.2}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(DoorSceneTest, UsingADoorOpensItThenItClosesByItself) {
	scene_.FinishLoading();
	player_.SetCommand(PlayerCommand{.use = true});
	scene_.Update(kTick);
	player_.SetCommand(PlayerCommand{});
	Run(Scene::kDoorMoveSeconds);
	EXPECT_DOUBLE_EQ(Openness(), 1.0);
	EXPECT_FALSE(scene_.GetMap().IsBlocked(1, 3));

	// Open for a while, then shut again
	Run(Scene::kDoorOpenSeconds - 1.0);
	EXPECT_DOUBLE_EQ(Openness(), 1.0);
	Run(1.0 + Scene::kDoorMoveSeconds + 0.1);
	EXPECT_DOUBLE_EQ(Openness(), 0.0);
	EXPECT_TRUE(scene_.GetMap().IsBlocked(1, 3));
}

TEST_F(DoorSceneTest, ADoorOutOfReachStaysShut) {
	scene_.FinishLoading();
	player_.SetPosition(Position2D({1.5, 1.2}, -kFacingDown));	// facing away
	player_.SetCommand(PlayerCommand{.use = true});
	Run(1.0);
	EXPECT_DOUBLE_EQ(Openness(), 0.0);
}

TEST_F(DoorSceneTest, ADoorNeverClosesOnSomeone) {
	scene_.FinishLoading();
	scene_.OpenDoor(0);
	Run(Scene::kDoorMoveSeconds + 0.1);
	player_.SetPosition(Position2D({1.5, 3.5}, kFacingDown));  // in the doorway
	Run(Scene::kDoorOpenSeconds * 2);
	EXPECT_DOUBLE_EQ(Openness(), 1.0);
}

TEST_F(DoorSceneTest, EnemiesOpenTheDoorsTheyReach) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({1.5, 4.6}, -kFacingDown)));
	scene_.FinishLoading();
	Run(Scene::kDoorMoveSeconds + 0.1);
	EXPECT_DOUBLE_EQ(Openness(), 1.0);
}

class LockedDoorTest : public DoorSceneTest
{
  protected:
	LockedDoorTest() : DoorSceneTest('G') {}
};

// Without its key a locked door stays shut, and says which key it needs
TEST_F(LockedDoorTest, OpensOnlyWithItsKey) {
	scene_.FinishLoading();
	player_.SetCommand(PlayerCommand{.use = true});
	Run(1.0);
	EXPECT_DOUBLE_EQ(Openness(), 0.0);
	EXPECT_EQ(scene_.GetNotice(), Scene::Notice::NeedGoldKey);

	player_.SetKeys(KeyBit(KeyColour::Silver));	 // the wrong key
	Run(1.0);
	EXPECT_DOUBLE_EQ(Openness(), 0.0);

	player_.SetKeys(KeyBit(KeyColour::Gold));
	Run(Scene::kDoorMoveSeconds + 0.1);
	EXPECT_DOUBLE_EQ(Openness(), 1.0);

	// The notice fades once the player stops trying
	player_.SetCommand(PlayerCommand{});
	Run(3.0);
	EXPECT_EQ(scene_.GetNotice(), Scene::Notice::None);
}

TEST_F(LockedDoorTest, EnemiesCannotOpenIt) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({1.5, 4.6}, -kFacingDown)));
	scene_.FinishLoading();
	Run(2.0);
	EXPECT_DOUBLE_EQ(Openness(), 0.0);
}

// A key is taken once, and kept
TEST_F(DoorSceneTest, AKeyIsPickedUpOnce) {
	const PickupEffect gold{.keys = KeyBit(KeyColour::Gold)};
	EXPECT_TRUE(player_.TryPickUp(gold));
	EXPECT_TRUE(player_.HasKey(KeyColour::Gold));
	EXPECT_FALSE(player_.HasKey(KeyColour::Silver));
	EXPECT_FALSE(player_.TryPickUp(gold)) << "already held";
}

#ifdef KARAKALE_COUNTS_ALLOCATIONS
TEST_F(DoorSceneTest, DoorsMoveWithoutAllocating) {
	scene_.FinishLoading();
	scene_.Update(kTick);
	const auto before = AllocationStats::count;
	player_.SetCommand(PlayerCommand{.use = true});
	Run(Scene::kDoorMoveSeconds + Scene::kDoorOpenSeconds + 1.0);
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace karakale
