// Enemies reuse their state objects, so every entry into a state must start
// from fresh per-visit data. This drives a real enemy through being hit
// twice and then killed.

#include "Characters/enemy.h"
#include "Characters/player.h"
#include "Core/scene.h"
#include "GameMap/map.h"
#include "Strike/weapon.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <memory>

namespace wolfenstein {
namespace {

class EnemyStateTest : public ::testing::Test
{
  protected:
	// How long after flinching it can flinch again
	const double kCooldown =
		testing::Enemy("soldier").behaviour.pain_cooldown_seconds;

	void SetUp() override {
		scene_.SetPlayer(player_);
		ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
									Position2D({1.5, 1.5}, 0.0)));
		enemy_ = scene_.GetEnemies().front();
		scene_.FinishLoading();
	}

	// Lands a shot: what ResolvePlayerShot does to the enemy it hits
	void Hit(double damage) {
		enemy_->DecreaseHealth(damage);
		enemy_->SetAttacked(true);
	}

	// The scene borrows the player, so the player is declared first
	CharacterConfig player_config_{Position2D({1.5, 4.5}, 0.0), 1.0, 1.0, 0.4,
								   0.4};
	Player player_{player_config_, testing::Weapon("mp5"),
				   testing::TestTextures(), testing::TestSound()};
	static constexpr SceneCapacity kCapacity{.enemies = 1};
	Map map_{testing::WriteMapFile(
				 "wolfenstein_enemy_state_test.txt",
				 // Two chambers split by a wall: the enemy cannot see the
				 // player, so only hits drive its state (a visible player
				 // would make an idle enemy walk first)
				 {"3333333", "3003003", "3003003", "3333333"})
				 .string()};
	memory::MonotonicArena arena_{Scene::MemoryFor(map_, kCapacity)};
	Scene scene_{testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_};
	Enemy* enemy_ = nullptr;
};

TEST_F(EnemyStateTest, PainIsReenteredWithAFreshTimer) {
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Idle);

	Hit(10.0);
	enemy_->Update(0.016);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);

	enemy_->Update(0.3);  // longer than the pain animation
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Walk);
	enemy_->Update(kCooldown);	// able to flinch again

	Hit(10.0);
	enemy_->Update(0.016);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
	// A timer left over from the first visit would end the pain right away
	enemy_->Update(0.1);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
}

// Hit again soon after flinching, it is hurt but does not flinch: steady
// fire must not keep an enemy from shooting back
TEST_F(EnemyStateTest, ItFlinchesOnlyOnceInACooldown) {
	Hit(10.0);
	enemy_->Update(0.016);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
	enemy_->Update(0.3);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Walk);

	const double health = enemy_->GetHealth();
	Hit(10.0);
	enemy_->Update(0.016);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Walk) << "no flinch";
	EXPECT_DOUBLE_EQ(enemy_->GetHealth(), health - 10.0) << "but hurt";
	// A hit it did not flinch at is spent: it does not flinch later for it
	enemy_->Update(kCooldown);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Walk);
}

// The killing hit always drops it, cooldown or not
TEST_F(EnemyStateTest, TheKillingHitIsNeverShrugged) {
	Hit(10.0);
	enemy_->Update(0.016);
	enemy_->Update(0.3);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Walk);
	Hit(enemy_->GetHealth());
	enemy_->Update(0.016);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
	enemy_->Update(0.3);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Death);
}

TEST_F(EnemyStateTest, AFatalHitLeadsToDeath) {
	Hit(150.0);
	enemy_->Update(0.016);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
	enemy_->Update(0.3);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Death);
}

// The complaint this answers: hitting an enemy every few tenths of a second
// kept it flinching, so it never fired. It must still hurt the player.
TEST(EnemyUnderFire, StillShootsBack) {
	CharacterConfig config{Position2D({1.5, 4.5}, 0.0), 1.0, 1.0, 0.4, 0.4};
	Player player(config, testing::Weapon("pistol"), testing::TestTextures(),
				  testing::TestSound());
	static constexpr SceneCapacity kCapacity{.enemies = 1};
	// A corridor along y (a map file's rows run along x): they see each other
	Map map(testing::WriteMapFile("wolfenstein_under_fire_test.txt",
								  {"333333", "300003", "333333"})
				.string());
	memory::MonotonicArena arena(Scene::MemoryFor(map, kCapacity));
	Scene scene(testing::TestTextures(), testing::TestSound(), map, kCapacity,
				arena);
	scene.SetPlayer(player);
	ASSERT_TRUE(
		scene.AddEnemy(testing::Enemy("soldier"), Position2D({1.5, 1.5}, 0.0)));
	scene.FinishLoading();
	Enemy& enemy = *scene.GetEnemies().front();

	constexpr double kTick = 1.0 / 60.0;
	for (int tick = 0; tick < 600; ++tick) {  // ten seconds
		if (tick % 18 == 0) {				  // a hit every 0.3 s
			enemy.DecreaseHealth(0.5);
			enemy.SetAttacked(true);
		}
		scene.Update(kTick);
	}
	ASSERT_TRUE(enemy.IsAlive());
	EXPECT_LT(player.GetHealth(), 100.0);
}

}  // namespace
}  // namespace wolfenstein
