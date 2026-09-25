// Owners and their states used to hold shared_ptrs to each other, so neither
// was ever destroyed. These tests pin down that dropping the last owner of a
// weapon or an enemy really destroys it.

#include "Characters/enemy.h"
#include "Core/scene.h"
#include "Strike/weapon.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <memory>

namespace wolfenstein {
namespace {

TEST(Lifetime, WeaponIsDestroyedWithItsLastOwner) {
	auto weapon = std::make_shared<Weapon>("mp5", testing::TestTextures(),
										   testing::TestSound());
	weapon->Reload();  // switch state once, as in play
	const std::weak_ptr<Weapon> observer = weapon;

	weapon.reset();
	EXPECT_TRUE(observer.expired());
}

// Enemies live in the scene's pool, inside the level arena; a level cannot
// hold more than it declared
Map RoomMap() {
	return Map(testing::WriteMapFile("wolfenstein_lifetime_test.txt",
									 {"33333", "30003", "30003", "33333"})
				   .string());
}

TEST(Lifetime, EnemiesLiveInTheLevelArena) {
	Scene scene(testing::TestTextures(), testing::TestSound(), RoomMap(),
				SceneCapacity{.enemies = 2});
	const std::size_t before = scene.LevelMemory().Used();
	const CharacterConfig config(Position2D({2.5, 2.5}, 0.0), 1.0, 1.0, 0.5,
								 0.5);
	ASSERT_TRUE(scene.AddEnemy("soldier", config));
	ASSERT_TRUE(scene.AddEnemy("soldier", config));
	EXPECT_EQ(scene.GetEnemies().size(), 2u);
	EXPECT_EQ(scene.LevelMemory().Used(), before);	// storage reserved up front

	const auto third = scene.AddEnemy("soldier", config);
	ASSERT_FALSE(third.has_value());
	EXPECT_EQ(third.error(), memory::PoolError::Full);
}

// The navigation grid, routes and scratch buffers are per-level data too: they
// come out of the same arena, which was sized for them, so building them
// neither throws nor touches the heap for them
TEST(Lifetime, NavigationLivesInTheLevelArena) {
	Scene scene(testing::TestTextures(), testing::TestSound(), RoomMap(),
				SceneCapacity{.enemies = 1});
	ASSERT_TRUE(scene.AddEnemy(
		"soldier",
		CharacterConfig(Position2D({1.5, 1.5}, 0.0), 1.0, 1.0, 0.4, 0.4)));
	const std::size_t before = scene.LevelMemory().Used();
	scene.FinishLoading();
	EXPECT_GT(scene.LevelMemory().Used(), before);
	EXPECT_LE(scene.LevelMemory().Used(), scene.LevelMemory().Capacity());
}

}  // namespace
}  // namespace wolfenstein
