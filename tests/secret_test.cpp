// Secrets: a wall that slides back when the player uses it, opening a hidden
// room behind

#include "Camera/raycaster.h"
#include "Core/scene.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// A room at y = 1..2 with a wall at (2, 3) in front of a hidden room at
// y = 4..6: the wall slides along +y
Map WithSecret(const char* name) {
	return Map(testing::WriteMapFile(name, {"33333333", "30033333", "30030003",
											"30033333", "33333333"})
				   .string());
}

TEST(Secret, AWallWithRoomBehindItCanBeASecret) {
	Map map = WithSecret("wolfenstein_secret_add_test.txt");
	EXPECT_TRUE(map.AddPushWall(2, 3, 0, 1));
	EXPECT_FALSE(map.AddPushWall(1, 3, 0, 1)) << "rock behind it";
	EXPECT_FALSE(map.AddPushWall(2, 2, 0, 1)) << "not a wall";
	ASSERT_EQ(map.GetPushWalls().size(), 1u);
	EXPECT_NE(map.FindPushWall(2, 3), nullptr);
}

TEST(Secret, ItSlidesTwoCellsAndStays) {
	Map map = WithSecret("wolfenstein_secret_slide_test.txt");
	ASSERT_TRUE(map.AddPushWall(2, 3, 0, 1));
	map.Push(0);
	map.AdvancePushWalls(0.5);
	// Its whole way is blocked while it moves
	for (const int y : {3, 4, 5}) {
		EXPECT_TRUE(map.IsBlocked(2, y)) << y;
	}
	EXPECT_EQ(map.FindPushWall(2, 3), nullptr) << "pushed only once";

	map.AdvancePushWalls(2.0);
	EXPECT_FALSE(map.GetPushWalls()[0].moving);
	EXPECT_FALSE(map.IsBlocked(2, 3));
	EXPECT_FALSE(map.IsBlocked(2, 4));
	EXPECT_TRUE(map.IsWall(2, 5)) << "where it ends";
	EXPECT_FALSE(map.IsBlocked(2, 6)) << "the room beyond it";
}

TEST(Secret, RaysMeetItWhereItIs) {
	Map map = WithSecret("wolfenstein_secret_ray_test.txt");
	ASSERT_TRUE(map.AddPushWall(2, 3, 0, 1));
	const Position2D eye({2.5, 1.5}, kFacingDown);
	EXPECT_NEAR(CastRay(map, eye, kFacingDown, 15.0).distance, 1.5, 1e-9);
	map.Push(0);
	map.AdvancePushWalls(0.75);	 // its face now at y = 3.75
	const Ray moving = CastRay(map, eye, kFacingDown, 15.0);
	ASSERT_TRUE(moving.is_hit);
	EXPECT_NEAR(moving.distance, 2.25, 1e-9);
	map.AdvancePushWalls(2.0);	// at rest, face at y = 5
	EXPECT_NEAR(CastRay(map, eye, kFacingDown, 15.0).distance, 3.5, 1e-9);
}

class SecretSceneTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.secrets = 1};

	SecretSceneTest()
		: map_(WithSecret("wolfenstein_secret_scene_test.txt")),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound()) {
		scene_.SetPlayer(player_);
		EXPECT_TRUE(scene_.GetMap().AddPushWall(2, 3, 0, 1));
		scene_.FinishLoading();
	}

	CharacterConfig config_{Position2D({2.5, 2.2}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(SecretSceneTest, UsingItPushesItAndCountsIt) {
	EXPECT_EQ(scene_.GetStats().secrets, 1u);
	EXPECT_EQ(scene_.GetStats().secrets_found, 0u);
	player_.SetCommand(PlayerCommand{.use = true});
	scene_.Update(kTick);
	player_.SetCommand(PlayerCommand{});
	EXPECT_EQ(scene_.GetNotice(), Scene::Notice::Secret);
	EXPECT_EQ(scene_.GetStats().secrets_found, 1u);
	for (double t = 0.0; t < Scene::kPushSeconds + 0.1; t += kTick) {
		scene_.Update(kTick);
	}
	EXPECT_FALSE(scene_.GetMap().GetPushWalls()[0].moving);
	EXPECT_TRUE(scene_.GetMap().IsWall(2, 5));
}

}  // namespace
}  // namespace wolfenstein
