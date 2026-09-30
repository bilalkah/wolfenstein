// Shots are resolved by the simulation from the game state (Aim), not by
// what the camera last drew

#include "Characters/player.h"
#include "Core/scene.h"
#include "ShootingManager/shooting_manager.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <string_view>
#include <utility>

namespace karakale {
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
		: map_(testing::WriteMapFile("karakale_aim_test.txt", rows).string()),
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

// Looking up or down, the shot climbs or falls as it flies (from the eye,
// half a wall up): the soldier four units off is 0.6 of a wall tall
TEST_F(AimTest, AShotPassesOverAHeadOrIntoTheFloor) {
	const Position2D eye({1.5, 1.5}, kFacingDown);
	const double tall = scene_.GetEnemies().front()->GetHeight();
	const auto at_height = [](double height) {
		return (height - kEyeHeight) / 4.0;	 // the pitch that meets it there
	};
	EXPECT_TRUE(Aim(scene_, eye, at_height(tall - 0.05)).is_hit) << "the head";
	EXPECT_TRUE(Aim(scene_, eye, at_height(0.05)).is_hit) << "the feet";
	EXPECT_FALSE(Aim(scene_, eye, at_height(tall + 0.05)).is_hit) << "over it";
	EXPECT_FALSE(Aim(scene_, eye, at_height(-0.05)).is_hit) << "the floor";
}

// A soldier whose frame has a shape: a head, a body, and two legs with a
// gap between them. Shots meet only what shows.
class ShapedTargetTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	ShapedTargetTest() {
		EXPECT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
									Position2D({1.5, 5.5}, 0.0)));
		scene_.FinishLoading();
		enemy_ = scene_.GetEnemies().front();
		static constexpr std::array<std::string_view, 10> kShape{
			"..........",  // the top row: nothing
			"....##....",  // the head
			"....##....",
			"..######..",  // the body
			"..######..", "..######..",
			"..##..##..",  // the legs, apart
			"..##..##..", "..##..##..", "..##..##.."};
		textures_.DefineMask(enemy_->GetTextureId(), kShape);
	}

	// The eye and pitch of a shot from (1.5, 1.5) at a point of the enemy's
	// frame, `across` from its left and `down` from its top (0 to 1)
	std::pair<Position2D, double> ShotAt(double across, double down) const {
		const vector2d centre = enemy_->GetPose();
		const vector2d eye{1.5, 1.5};
		const vector2d towards = (centre - eye) / centre.Distance(eye);
		const vector2d right{-towards.y, towards.x};
		const vector2d point =
			centre + right * ((across - 0.5) * enemy_->GetWidth());
		const double along = point.Distance(eye);
		const double height = (1.0 - down) * enemy_->GetHeight();
		return {Position2D(eye, std::atan2(point.y - eye.y, point.x - eye.x)),
				(height - kEyeHeight) / along};
	}
	bool Hits(double across, double down) const {
		const auto [eye, pitch] = ShotAt(across, down);
		return Aim(scene_, eye, pitch).is_hit;
	}
	// The damage a pistol shot there does
	double DamageAt(double across, double down) {
		const auto [eye, pitch] = ShotAt(across, down);
		const double before = enemy_->GetHealth();
		ResolvePlayerShot(scene_, player_.GetWeapon(), eye, pitch);
		const double damage = before - enemy_->GetHealth();
		enemy_->IncreaseHealth(damage);
		return damage;
	}

	TextureManager textures_;
	// Before anything below looks a clip up
	bool defined_ = (testing::DefineTestTextures(textures_), true);
	Map map_{testing::WriteMapFile("karakale_shaped_target_test.txt",
								   {"3333333", "3000003", "3333333"})
				 .string()};
	memory::MonotonicArena arena_{Scene::MemoryFor(map_, kCapacity)};
	Scene scene_{textures_, testing::TestSound(), map_, kCapacity, arena_};
	CharacterConfig config_{Position2D({1.5, 1.5}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Player player_{config_, testing::Weapon("pistol"), textures_,
				   testing::TestSound()};
	Enemy* enemy_ = nullptr;
};

TEST_F(ShapedTargetTest, AShotMeetsOnlyWhatShows) {
	EXPECT_TRUE(Hits(0.45, 0.15)) << "the head";
	EXPECT_FALSE(Hits(0.25, 0.15)) << "beside the head";
	EXPECT_FALSE(Hits(0.45, 0.05)) << "over it";
	EXPECT_TRUE(Hits(0.3, 0.45)) << "the body";
	EXPECT_TRUE(Hits(0.3, 0.75)) << "a leg";
	EXPECT_FALSE(Hits(0.5, 0.75)) << "between the legs";
}

// The head hurts most, the legs least: the zones are shares of the figure
// (what shows, rows 1 to 9), not of the frame
TEST_F(ShapedTargetTest, TheHeadHurtsMostAndTheLegsLeast) {
	const HitZones& zones = enemy_->GetHitZones();
	const double body = DamageAt(0.3, 0.45);
	ASSERT_GT(body, 0.0);
	EXPECT_NEAR(DamageAt(0.45, 0.15), body * zones.head_damage, 1e-6);
	EXPECT_NEAR(DamageAt(0.3, 0.85), body * zones.leg_damage, 1e-6);
	EXPECT_DOUBLE_EQ(DamageAt(0.5, 0.75), 0.0) << "between the legs";
}

// A shot says what it hit, and the head bleeds a bigger burst
TEST_F(ShapedTargetTest, AShotSaysWhereItHit) {
	const auto shoot = [&](double across, double down) {
		const auto [eye, pitch] = ShotAt(across, down);
		return ResolvePlayerShot(scene_, player_.GetWeapon(), eye, pitch);
	};
	const auto biggest_puff = [&] {
		double size = 0.0;
		for (const IGameObject* object : scene_.GetObjects()) {
			if (object->GetObjectType() == ObjectType::EFFECT &&
				object->IsVisible()) {
				size = std::max(size, object->GetHeight());
			}
		}
		return size;
	};
	const ShotResult body = shoot(0.3, 0.45);
	EXPECT_TRUE(body.hit);
	EXPECT_FALSE(body.head);
	const double body_puff = biggest_puff();
	const ShotResult head = shoot(0.45, 0.15);
	EXPECT_TRUE(head.hit);
	EXPECT_TRUE(head.head);
	EXPECT_GT(biggest_puff(), body_puff) << "a bigger burst";
	const ShotResult miss = shoot(0.5, 0.75);
	EXPECT_FALSE(miss.hit) << "between the legs";
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
}  // namespace karakale
