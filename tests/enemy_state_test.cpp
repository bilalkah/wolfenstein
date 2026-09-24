// Enemies reuse their state objects, so every entry into a state must start
// from fresh per-visit data. This drives a real enemy through being hit
// twice and then killed.

#include "Camera/single_raycaster.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "Map/map.h"
#include "NavigationManager/navigation_manager.h"
#include "test_map.h"
#include <gtest/gtest.h>
#include <memory>

namespace wolfenstein {
namespace {

class EnemyStateTest : public ::testing::Test
{
  protected:
	void SetUp() override {
		scene_->SetMap(std::make_shared<Map>(
			testing::WriteMapFile("wolfenstein_enemy_state_test.txt",
								  // Two chambers split by a wall: the enemy
								  // cannot see the player, so only hits drive
								  // its state (a visible player would make an
								  // idle enemy walk first)
								  {"3333333", "3003003", "3003003", "3333333"})
				.string()));
		enemy_ = EnemyFactory::CreateEnemy(
			"soldier",
			CharacterConfig(Position2D({1.5, 1.5}, 0.0), 1.0, 1.0, 0.4, 0.4));
		scene_->AddObject(enemy_);

		SingleRayCasterService::GetInstance().InitService(scene_);
		SingleRayCasterService::GetInstance().SetDestinationPtr(player_);
		NavigationManager::GetInstance().InitManager(scene_);
		NavigationManager::GetInstance().SetPositionPtr(player_);
	}

	// Lands a shot: what ShootingManager does to the enemy it hits
	void Hit(double damage) {
		enemy_->DecreaseHealth(damage);
		enemy_->SetAttacked(true);
	}

	std::shared_ptr<Scene> scene_ = std::make_shared<Scene>();
	std::shared_ptr<Position2D> player_ =
		std::make_shared<Position2D>(vector2d{1.5, 4.5}, 0.0);
	std::shared_ptr<Enemy> enemy_;
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
