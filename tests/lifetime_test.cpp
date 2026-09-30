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

namespace karakale {
namespace {

TEST(Lifetime, WeaponIsDestroyedWithItsLastOwner) {
	auto weapon = std::make_shared<Weapon>(
		testing::Weapon("mp5"), testing::TestTextures(), testing::TestSound());
	weapon->Reload();  // switch state once, as in play
	const std::weak_ptr<Weapon> observer = weapon;

	weapon.reset();
	EXPECT_TRUE(observer.expired());
}

// Enemies live in the scene's pool, inside the level arena; a level cannot
// hold more than it declared
Map RoomMap() {
	return Map(testing::WriteMapFile("karakale_lifetime_test.txt",
									 {"33333", "30003", "30003", "33333"})
				   .string());
}

TEST(Lifetime, EnemiesLiveInTheLevelArena) {
	const Map map = RoomMap();
	constexpr SceneCapacity kCapacity{.enemies = 2};
	memory::MonotonicArena arena(Scene::MemoryFor(map, kCapacity));
	Scene scene(testing::TestTextures(), testing::TestSound(), map, kCapacity,
				arena);
	const std::size_t before = scene.LevelMemory().Used();
	const EnemyConfig& soldier = testing::Enemy("soldier");
	const Position2D spawn({2.5, 2.5}, 0.0);
	ASSERT_TRUE(scene.AddEnemy(soldier, spawn));
	ASSERT_TRUE(scene.AddEnemy(soldier, spawn));
	EXPECT_EQ(scene.GetEnemies().size(), 2u);
	EXPECT_EQ(scene.LevelMemory().Used(), before);	// storage reserved up front

	const auto third = scene.AddEnemy(soldier, spawn);
	ASSERT_FALSE(third.has_value());
	EXPECT_EQ(third.error(), memory::PoolError::Full);
}

// The navigation grid, routes and scratch buffers are per-level data too: they
// come out of the same arena, which was sized for them, so building them
// neither throws nor touches the heap for them
TEST(Lifetime, NavigationLivesInTheLevelArena) {
	const Map map = RoomMap();
	constexpr SceneCapacity kCapacity{.enemies = 1};
	memory::MonotonicArena arena(Scene::MemoryFor(map, kCapacity));
	Scene scene(testing::TestTextures(), testing::TestSound(), map, kCapacity,
				arena);
	ASSERT_TRUE(
		scene.AddEnemy(testing::Enemy("soldier"), Position2D({1.5, 1.5}, 0.0)));
	const std::size_t before = scene.LevelMemory().Used();
	scene.FinishLoading();
	EXPECT_GT(scene.LevelMemory().Used(), before);
	EXPECT_LE(scene.LevelMemory().Used(), scene.LevelMemory().Capacity());
}

}  // namespace
}  // namespace karakale
