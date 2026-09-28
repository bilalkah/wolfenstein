// Gunfire carries, the player's and the enemies': it spreads through open
// floor, round corners but not through walls or closed doors, and enemies it
// reaches come hunting

#include "Core/scene.h"
#include "Profiler/profiler.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
// A map file's rows run along x. Two corridors joined at their far end:
// from (1, 1) to (3, 1) is 2 cells straight through the wall, 16 round.
constexpr std::initializer_list<const char*> kBend = {
	"3333333333",  //
	"3000000003",  // the player's corridor
	"3333333303",  // the way round
	"3000000003",  // the other corridor
	"3333333333"};
constexpr std::initializer_list<const char*> kSealed = {"33333", "30303",
														"33333"};
constexpr std::initializer_list<const char*> kDoor = {"33333333", "3000D003",
													  "33333333"};

// The player at (1.5, 1.5), one soldier somewhere on the map
class HearingTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	void Load(std::initializer_list<const char*> rows, vector2d enemy) {
		map_ = std::make_unique<Map>(
			testing::WriteMapFile("wolfenstein_hearing_test.txt", rows)
				.string());
		arena_ = std::make_unique<memory::MonotonicArena>(
			Scene::MemoryFor(*map_, kCapacity));
		scene_ = std::make_unique<Scene>(testing::TestTextures(),
										 testing::TestSound(), *map_, kCapacity,
										 *arena_);
		scene_->SetPlayer(player_);
		ASSERT_TRUE(scene_->AddEnemy(testing::Enemy("soldier"),
									 Position2D(enemy, 0.0)));
		scene_->FinishLoading();
		enemy_ = scene_->GetEnemies().front();
	}
	void Noise(int range) { scene_->MakeNoise({1.5, 1.5}, range); }

	CharacterConfig config_{Position2D({1.5, 1.5}, std::numbers::pi / 2), 2.0,
							0.4, 0.4, 1.0};
	Player player_{config_, testing::GameData().weapons, 0,
				   testing::TestTextures(), testing::TestSound()};
	std::unique_ptr<Map> map_;
	std::unique_ptr<memory::MonotonicArena> arena_;
	std::unique_ptr<Scene> scene_;
	Enemy* enemy_ = nullptr;
};

TEST_F(HearingTest, ItGoesRoundCornersNotThroughWalls) {
	Load(kBend, {3.5, 1.5});
	Noise(15);
	EXPECT_FALSE(enemy_->IsAlerted()) << "16 cells round the bend";
	Noise(16);
	EXPECT_TRUE(enemy_->IsAlerted());
}

TEST_F(HearingTest, ASealedRoomHearsNothing) {
	Load(kSealed, {1.5, 3.5});
	Noise(100);
	EXPECT_FALSE(enemy_->IsAlerted());
}

TEST_F(HearingTest, AClosedDoorStopsIt) {
	Load(kDoor, {1.5, 6.5});
	Noise(20);
	EXPECT_FALSE(enemy_->IsAlerted());
	scene_->OpenDoor(0);
	for (int tick = 0; tick < 60; ++tick) {
		scene_->Update(kTick);
	}
	Noise(20);
	EXPECT_TRUE(enemy_->IsAlerted());
}

// The player's own shot: the pistol carries 10 cells, and the soldier round
// the bend at (3, 7) is 10 away; it cannot see the player, but comes
TEST_F(HearingTest, AShotBringsThemHunting) {
	ASSERT_EQ(testing::Weapon("pistol").noise_range, 10);
	Load(kBend, {3.5, 7.5});
	scene_->Update(kTick);
	ASSERT_EQ(enemy_->GetStateType(), EnemyStateType::Idle);
	player_.SetCommand({.fire = true});
	scene_->Update(kTick);
	player_.SetCommand({});
	EXPECT_TRUE(enemy_->IsAlerted());
	scene_->Update(kTick);
	EXPECT_EQ(enemy_->GetStateType(), EnemyStateType::Walk);
}

// Alerted, it hunts only for a while
TEST_F(HearingTest, TheAlertWearsOff) {
	Load(kSealed, {1.5, 3.5});
	enemy_->Alert();
	const double seconds = testing::Enemy("soldier").behaviour.alert_seconds;
	for (int tick = 0; tick < static_cast<int>(seconds / kTick) + 2; ++tick) {
		scene_->Update(kTick);
	}
	EXPECT_FALSE(enemy_->IsAlerted());
}

// An enemy's shot carries too: a soldier in the player's corridor opens
// fire, and one round the bend at (3, 7), 7 cells away by the corridors and
// out of the player's sight, comes to the fight
TEST(EnemyGunfire, BringsTheOthersHunting) {
	ASSERT_GE(testing::Enemy("soldier").weapon.noise_range, 7);
	constexpr SceneCapacity kCapacity{.enemies = 2};
	const Map map(
		testing::WriteMapFile("wolfenstein_gunfire_test.txt", kBend).string());
	memory::MonotonicArena arena(Scene::MemoryFor(map, kCapacity));
	Scene scene(testing::TestTextures(), testing::TestSound(), map, kCapacity,
				arena);
	CharacterConfig config{Position2D({1.5, 1.5}, std::numbers::pi / 2), 2.0,
						   0.4, 0.4, 1.0};
	Player player(config, testing::GameData().weapons, 0,
				  testing::TestTextures(), testing::TestSound());
	scene.SetPlayer(player);
	ASSERT_TRUE(scene.AddEnemy(testing::Enemy("soldier"),
							   Position2D({1.5, 4.5}, -std::numbers::pi / 2)));
	ASSERT_TRUE(
		scene.AddEnemy(testing::Enemy("soldier"), Position2D({3.5, 7.5}, 0.0)));
	scene.FinishLoading();
	const Enemy& listener = *scene.GetEnemies()[1];

	// Until the first shot lands, the other stands idle, hearing nothing
	for (int tick = 0; tick < 300 && player.GetHealth() == 100.0; ++tick) {
		EXPECT_FALSE(listener.IsAlerted()) << tick;
		scene.Update(kTick);
	}
	ASSERT_LT(player.GetHealth(), 100.0) << "the soldier never fired";
	EXPECT_TRUE(listener.IsAlerted());
	scene.Update(kTick);
	EXPECT_EQ(listener.GetStateType(), EnemyStateType::Walk);
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
TEST_F(HearingTest, MakingANoiseAllocatesNothing) {
	Load(kBend, {3.5, 1.5});
	const auto before = AllocationStats::count;
	for (int shot = 0; shot < 100; ++shot) {
		Noise(20);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace wolfenstein
