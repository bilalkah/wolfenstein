// Shots are resolved by the simulation from the game state (Aim), not by
// what the camera last drew

#include "Core/scene.h"
#include "ShootingManager/shooting_manager.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// One soldier in a corridor, straight down from the shooter at (1.5, 1.5)
class AimTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	explicit AimTest(std::initializer_list<const char*> rows = {"3333333",
																"3000003",
																"3333333"})
		: map_(
			  testing::WriteMapFile("wolfenstein_aim_test.txt", rows).string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_) {
		EXPECT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
									Position2D({1.5, 5.5}, 0.0)));
	}
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
};

TEST_F(AimTest, HitsTheEnemyInTheLineOfFire) {
	const Ray aim = Aim(scene_, Position2D({1.5, 1.5}, kFacingDown));
	ASSERT_TRUE(aim.is_hit);
	EXPECT_EQ(aim.object_id, scene_.GetEnemies().front()->GetId());
	EXPECT_NEAR(aim.distance, 4.0, 1e-9);
}

TEST_F(AimTest, MissesWhenAimingAside) {
	EXPECT_FALSE(Aim(scene_, Position2D({1.5, 1.5}, kFacingDown + 0.5)).is_hit);
}

// Shot dead, an enemy falls for a while (pain, then its death) before it
// stops being alive. Shots at it meanwhile must not count it again: a count
// below zero wrapped round, and the level never ended.
TEST_F(AimTest, AFallingEnemyIsKilledOnce) {
	const Weapon weapon(testing::Weapon("mp5"), testing::TestTextures(),
						testing::TestSound());
	const Position2D eye({1.5, 1.5}, kFacingDown);
	ASSERT_EQ(scene_.GetNumberOfAliveEnemies(), 1u);
	for (int shot = 0; shot < 40; ++shot) {	 // many more than it takes
		ResolvePlayerShot(scene_, weapon, eye);
	}
	const Enemy& enemy = *scene_.GetEnemies().front();
	EXPECT_LE(enemy.GetHealth(), 0.0);
	EXPECT_TRUE(enemy.IsAlive());  // still falling: no update ran
	EXPECT_EQ(scene_.GetNumberOfAliveEnemies(), 0u);
	// Shots pass through it now
	EXPECT_FALSE(Aim(scene_, eye).is_hit);
}

// An enemy's shot hurts in proportion to the difficulty's damage
TEST(EnemyShot, TheDifficultyScalesItsDamage) {
	CharacterConfig config(Position2D({1.5, 1.5}, 0.0), 2.0, 0.4, 0.4, 1.0);
	Player normal(config, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound());
	Player hard(config, testing::Weapon("mp5"), testing::TestTextures(),
				testing::TestSound());
	const SimpleWeapon weapon(testing::Enemy("soldier").weapon);
	ResolveEnemyShot(normal, weapon);
	ResolveEnemyShot(hard, weapon, 2.0);
	EXPECT_DOUBLE_EQ(100.0 - hard.GetHealth(),
					 2 * (100.0 - normal.GetHealth()));
	EXPECT_LT(normal.GetHealth(), 100.0);
}

class AimThroughWallTest : public AimTest
{
  protected:
	AimThroughWallTest()
		: AimTest({"3333333", "3003003", "3333333"}) {}	 // wall at y = 3
};

TEST_F(AimThroughWallTest, AWallStopsTheShot) {
	EXPECT_FALSE(Aim(scene_, Position2D({1.5, 1.5}, kFacingDown)).is_hit);
}

}  // namespace
}  // namespace wolfenstein
