// The player carries every weapon found, each with its own rounds, and
// switches between them; the knife needs no ammunition but reaches only the
// enemy in front of it

#include "Core/scene.h"
#include "Core/world.h"
#include "Profiler/profiler.h"
#include "ShootingManager/shooting_manager.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <memory>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y
constexpr std::size_t kKnife = 0, kPistol = 1, kMp5 = 2, kShotgun = 3;

// A corridor with the player at (1.5, 1.5) facing down it, carrying the
// game's arsenal
class ArsenalTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	ArsenalTest()
		: map_(testing::WriteMapFile("wolfenstein_arsenal_test.txt",
									 {"33333333", "30000003", "33333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::GameData().weapons, kPistol,
				  testing::TestTextures(), testing::TestSound()) {
		scene_.SetPlayer(player_);
	}

	void Command(PlayerCommand command) {
		player_.SetCommand(command);
		scene_.Update(kTick);
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(ArsenalTest, AGameStartsWithTheKnifeAndThePistol) {
	EXPECT_EQ(player_.WeaponCount(), 4u);
	EXPECT_TRUE(player_.Owns(kKnife));
	EXPECT_TRUE(player_.Owns(kPistol));
	EXPECT_FALSE(player_.Owns(kMp5));
	EXPECT_FALSE(player_.Owns(kShotgun));
	EXPECT_EQ(player_.HeldWeapon(), kPistol);
	EXPECT_EQ(player_.GetWeapon().GetWeaponName(), "pistol");
}

TEST_F(ArsenalTest, OnlyWeaponsCarriedCanBeTakenInHand) {
	Command({.weapon = kKnife});
	EXPECT_EQ(player_.HeldWeapon(), kKnife);
	Command({.weapon = kShotgun});	// not carried
	EXPECT_EQ(player_.HeldWeapon(), kKnife);
}

// The wheel steps through the weapons carried, skipping the rest, round
TEST_F(ArsenalTest, TheWheelStepsThroughWeaponsCarried) {
	player_.SetOwnedWeapons(0b1011);  // knife, pistol, shotgun
	Command({.cycle = 1});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun) << "past the MP5 not carried";
	Command({.cycle = 1});
	EXPECT_EQ(player_.HeldWeapon(), kKnife) << "round to the first";
	Command({.cycle = -1});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun);
}

TEST_F(ArsenalTest, AWeaponFoundIsTakenInHand) {
	const PickupEffect mp5{.weapons = 1U << kMp5};
	ASSERT_TRUE(player_.TryPickUp(mp5));
	EXPECT_TRUE(player_.Owns(kMp5));
	EXPECT_EQ(player_.HeldWeapon(), kMp5);
	const WeaponConfig& config = testing::Weapon("mp5");
	EXPECT_EQ(player_.GetWeapon().GetAmmo(), config.ammo_capacity);
	EXPECT_EQ(player_.GetWeapon().GetReserve(), config.reserve_start);

	// Found again, it gives a box of its rounds
	ASSERT_TRUE(player_.TryPickUp(mp5));
	EXPECT_EQ(player_.GetWeapon().GetReserve(),
			  config.reserve_start + config.box_rounds);
}

TEST_F(ArsenalTest, AnAmmoBoxTopsUpEveryFirearmCarried) {
	player_.SetOwnedWeapons(0b0111);  // knife, pistol, MP5
	const std::size_t pistol = player_.GetWeapon(kPistol).GetReserve();
	const std::size_t mp5 = player_.GetWeapon(kMp5).GetReserve();
	ASSERT_TRUE(player_.TryPickUp({.ammo_boxes = 1}));
	EXPECT_EQ(player_.GetWeapon(kPistol).GetReserve(),
			  pistol + testing::Weapon("pistol").box_rounds);
	EXPECT_EQ(player_.GetWeapon(kMp5).GetReserve(),
			  mp5 + testing::Weapon("mp5").box_rounds);
	EXPECT_EQ(player_.GetWeapon(kShotgun).GetReserve(),
			  testing::Weapon("shotgun").reserve_start)
		<< "not carried";
	EXPECT_EQ(player_.GetWeapon(kKnife).GetReserve(), 0u);
}

// The knife never runs out and never reloads
TEST_F(ArsenalTest, TheKnifeNeedsNoAmmunition) {
	Command({.weapon = kKnife});
	const Weapon& knife = player_.GetWeapon();
	ASSERT_TRUE(knife.IsMelee());
	// Stabbing and trying to reload for ten seconds
	for (int tick = 0; tick < 600; ++tick) {
		player_.SetCommand({.fire = true, .reload = tick % 50 == 0});
		scene_.Update(kTick);
	}
	EXPECT_EQ(knife.GetAmmo(), 0u);
	EXPECT_EQ(knife.GetReserve(), 0u);
	// Once the last stab is over it stabs again: never out of ammunition,
	// never reloading
	Command({});
	scene_.Update(1.0);
	EXPECT_TRUE(player_.GetWeapon(kKnife).Attack());
}

// A blade reaches the enemy in front of it and no further
TEST_F(ArsenalTest, TheKnifeReachesOnlyTheEnemyInFront) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({1.5, 4.5}, -kFacingDown)));
	scene_.FinishLoading();
	Enemy& enemy = *scene_.GetEnemies().front();
	const Weapon& knife = player_.GetWeapon(kKnife);
	ResolvePlayerShot(scene_, knife, Position2D({1.5, 1.5}, kFacingDown));
	EXPECT_DOUBLE_EQ(enemy.GetHealth(), 100.0) << "three cells away";
	ResolvePlayerShot(scene_, knife, Position2D({1.5, 3.6}, kFacingDown));
	EXPECT_LT(enemy.GetHealth(), 100.0) << "within reach";
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
TEST_F(ArsenalTest, SwitchingWeaponsAllocatesNothing) {
	player_.SetOwnedWeapons(0b1111);
	const auto before = AllocationStats::count;
	for (int step = 0; step < 40; ++step) {
		Command({.weapon = static_cast<std::int8_t>(step % 4),
				 .cycle = static_cast<std::int8_t>(step % 3 - 1)});
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

// A saved game keeps every weapon carried, with its rounds, and the one in
// hand
TEST(Arsenal, ASavedGameKeepsTheArsenal) {
	auto loader = SceneLoader::Open(RESOURCE_DIR);
	ASSERT_TRUE(loader);
	World world(testing::TestTextures(), std::move(*loader),
				std::make_unique<SoundManager>());
	ASSERT_TRUE(world.NewGame({}));
	Player& player = world.GetPlayer();
	ASSERT_TRUE(player.TryPickUp({.weapons = 1U << kShotgun}));
	player.GetWeapon(kPistol).SetRounds(3, 11);
	player.GetWeapon(kShotgun).SetRounds(1, 7);
	const auto saved = world.Capture();
	ASSERT_TRUE(saved);

	ASSERT_TRUE(world.ContinueGame(saved.value_or(SavedGame{})));
	const Player& back = world.GetPlayer();
	EXPECT_TRUE(back.Owns(kShotgun));
	EXPECT_FALSE(back.Owns(kMp5));
	EXPECT_EQ(back.HeldWeapon(), kShotgun);
	EXPECT_EQ(back.GetWeapon(kPistol).GetAmmo(), 3u);
	EXPECT_EQ(back.GetWeapon(kPistol).GetReserve(), 11u);
	EXPECT_EQ(back.GetWeapon(kShotgun).GetAmmo(), 1u);
	EXPECT_EQ(back.GetWeapon(kShotgun).GetReserve(), 7u);
}

}  // namespace
}  // namespace wolfenstein
