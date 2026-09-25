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
	Player player_{
		player_config_,
		std::make_shared<Weapon>(testing::Weapon("mp5"),
								 testing::TestTextures(), testing::TestSound()),
		testing::TestSound()};
	Scene scene_{testing::TestTextures(), testing::TestSound(),
				 Map(testing::WriteMapFile(
						 "wolfenstein_enemy_state_test.txt",
						 // Two chambers split by a wall: the enemy cannot see
						 // the player, so only hits drive its state (a visible
						 // player would make an idle enemy walk first)
						 {"3333333", "3003003", "3003003", "3333333"})
						 .string()),
				 SceneCapacity{.enemies = 1}};
	Enemy* enemy_ = nullptr;
};

TEST_F(EnemyStateTest, PainIsReenteredWithAFreshTimer) {
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Idle);

	Hit(30.0);
	enemy_->Update(0.016);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);

	enemy_->Update(0.3);  // longer than the pain animation
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Walk);

	Hit(30.0);
	enemy_->Update(0.016);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
	// A timer left over from the first visit would end the pain right away
	enemy_->Update(0.1);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
}

TEST_F(EnemyStateTest, AFatalHitLeadsToDeath) {
	Hit(150.0);
	enemy_->Update(0.016);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Pain);
	enemy_->Update(0.3);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Death);
}

}  // namespace
}  // namespace wolfenstein
