// Secrets: a wall that slides back when the player uses it, opening a hidden
// room behind

#include "Camera/camera.h"
#include "Camera/raycaster.h"
#include "Core/scene.h"
#include "Graphics/renderer_3d.h"
#include "Graphics/renderer_interface.h"
#include "Profiler/profiler.h"
#include "Settings/settings.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace karakale {
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
	Map map = WithSecret("karakale_secret_add_test.txt");
	EXPECT_TRUE(map.AddPushWall(2, 3, 0, 1));
	EXPECT_FALSE(map.AddPushWall(1, 3, 0, 1)) << "rock behind it";
	EXPECT_FALSE(map.AddPushWall(2, 2, 0, 1)) << "not a wall";
	ASSERT_EQ(map.GetPushWalls().size(), 1u);
	EXPECT_NE(map.FindPushWall(2, 3), nullptr);
}

TEST(Secret, ItSlidesTwoCellsAndStays) {
	Map map = WithSecret("karakale_secret_slide_test.txt");
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
	Map map = WithSecret("karakale_secret_ray_test.txt");
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
		: map_(WithSecret("karakale_secret_scene_test.txt")),
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

#ifdef KARAKALE_COUNTS_ALLOCATIONS
// Face to face with a secret wall, every column draws the wall and many its
// mark too: the draw queue has room for both
TEST_F(SecretSceneTest, DrawingItsMarkAllocatesNothing) {
	Camera2D camera(Camera2DConfig(1200, std::numbers::pi / 3, 15.0));
	RendererContext context("secret test",
							RenderConfig(1200, 900, 0, 32, 0, 15.0, 1.0, false),
							camera);
	camera.SetScene(scene_);
	Renderer3D renderer(context);
	renderer.ReserveObjects(scene_.GetObjects().size());
	renderer.SetScene(scene_);
	// The game reads its settings at startup, before any frame
	(void)Settings::Get();
	// Counted from the first frame: a queue too small grows only once, the
	// first time a secret wall comes into view
	const Position2D eye({2.5, 2.6}, kFacingDown);	// 0.4 from its face
	const auto before = AllocationStats::count;
	for (int frame = 0; frame < 10; ++frame) {
		camera.Update(eye, 1.0);
		renderer.RenderScene(kTick);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

// The secret's scene with a soldier standing at `enemy`
class SecretWayTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1, .secrets = 1};

	void Load(vector2d enemy) {
		scene_.SetPlayer(player_);
		ASSERT_TRUE(scene_.GetMap().AddPushWall(2, 3, 0, 1));
		ASSERT_TRUE(
			scene_.AddEnemy(testing::Enemy("soldier"), Position2D(enemy, 0.0)));
		scene_.FinishLoading();
	}
	void Use() {
		player_.SetCommand(PlayerCommand{.use = true});
		scene_.Update(kTick);
		player_.SetCommand(PlayerCommand{});
	}
	// Where a way from `from` into the hidden room (the part the wall slides
	// out of) leads first; `from` if there is none
	vector2d WayIn(vector2d from) {
		const Enemy& enemy = *scene_.GetEnemies().front();
		return scene_.GetNavigation().FindPath(
			Position2D(from, 0.0), Position2D({2.5, 4.5}, 0.0), enemy.GetId());
	}

	CharacterConfig config_{Position2D({2.5, 2.2}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_{WithSecret("karakale_secret_way_test.txt")};
	memory::MonotonicArena arena_{Scene::MemoryFor(map_, kCapacity)};
	Scene scene_{testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_};
	Player player_{config_, testing::Weapon("mp5"), testing::TestTextures(),
				   testing::TestSound()};
};

// Pushed, it opens the hidden room to the enemies' ways too
TEST_F(SecretWayTest, PushedItOpensTheWayForEnemies) {
	Load({1.5, 1.5});
	const vector2d from{1.5, 1.5};
	EXPECT_EQ(WayIn(from), from) << "sealed";
	Use();
	ASSERT_TRUE(scene_.GetMap().GetPushWalls()[0].pushed);
	EXPECT_EQ(WayIn(from), from) << "not through it while it slides";
	for (double t = 0.0; t < Scene::kPushSeconds + 0.1; t += kTick) {
		scene_.Update(kTick);
	}
	EXPECT_NE(WayIn(from), from) << "open once it stops";
}

// An enemy standing in its way keeps it shut
TEST_F(SecretWayTest, AnEnemyInTheWayKeepsItShut) {
	Load({2.5, 4.5});
	Use();
	EXPECT_FALSE(scene_.GetMap().GetPushWalls()[0].pushed);
	EXPECT_NE(scene_.GetNotice(), Scene::Notice::Secret);
}

}  // namespace
}  // namespace karakale
