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
	explicit AimTest(std::initializer_list<const char*> rows = {"3333333",
																"3000003",
																"3333333"})
		: scene_(testing::TestTextures(), testing::TestSound(),
				 Map(testing::WriteMapFile("wolfenstein_aim_test.txt", rows)
						 .string()),
				 SceneCapacity{.enemies = 1}) {
		EXPECT_TRUE(scene_.AddEnemy(
			"soldier",
			CharacterConfig(Position2D({1.5, 5.5}, 0.0), 1.0, 1.0, 0.4, 0.4)));
	}
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
