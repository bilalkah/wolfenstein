// Owners and their states used to hold shared_ptrs to each other, so neither
// was ever destroyed. These tests pin down that dropping the last owner of a
// weapon or an enemy really destroys it.

#include "Characters/enemy.h"
#include "Strike/weapon.h"
#include <gtest/gtest.h>
#include <memory>

namespace wolfenstein {
namespace {

TEST(Lifetime, WeaponIsDestroyedWithItsLastOwner) {
	auto weapon = std::make_shared<Weapon>("mp5");
	weapon->Reload();  // switch state once, as in play
	const std::weak_ptr<Weapon> observer = weapon;

	weapon.reset();
	EXPECT_TRUE(observer.expired());
}

TEST(Lifetime, EnemyIsDestroyedWithItsLastOwner) {
	CharacterConfig config(Position2D({2.5, 2.5}, 0.0), 1.0, 1.0, 0.5, 0.5);
	auto enemy = EnemyFactory::CreateEnemy("soldier", config);
	const std::weak_ptr<Enemy> observer = enemy;

	enemy.reset();
	EXPECT_TRUE(observer.expired());
}

}  // namespace
}  // namespace wolfenstein
