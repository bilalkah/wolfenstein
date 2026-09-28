// The player carries every weapon found, each with its own rounds, and
// switches between them; a melee weapon (none ships, but the engine takes
// one: a test blade here) needs no ammunition but reaches only the enemy in
// front of it

#include "Core/scene.h"
#include "Core/world.h"
#include "Profiler/profiler.h"
#include "ShootingManager/shooting_manager.h"
#include "test_map.h"
#include "test_services.h"
#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y
constexpr std::size_t kPistol = 0, kMp5 = 1, kShotgun = 2;

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
	// Until a weapon just taken in hand is up
	void WaitForTheWeapon() {
		player_.SetCommand({});
		for (int tick = 0; tick < 30; ++tick) {
			scene_.Update(kTick);
		}
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(ArsenalTest, AGameStartsWithThePistol) {
	EXPECT_EQ(player_.WeaponCount(), 3u);
	EXPECT_TRUE(player_.Owns(kPistol));
	EXPECT_FALSE(player_.Owns(kMp5));
	EXPECT_FALSE(player_.Owns(kShotgun));
	EXPECT_EQ(player_.HeldWeapon(), kPistol);
	EXPECT_EQ(player_.GetWeapon().GetWeaponName(), "pistol");
}

TEST_F(ArsenalTest, OnlyWeaponsCarriedCanBeTakenInHand) {
	player_.SetOwnedWeapons(0b101);	 // pistol, shotgun
	Command({.weapon = kShotgun});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun);
	Command({.weapon = kMp5});	// not carried
	EXPECT_EQ(player_.HeldWeapon(), kShotgun);
}

// The wheel steps through the weapons carried, skipping the rest, round
TEST_F(ArsenalTest, TheWheelStepsThroughWeaponsCarried) {
	player_.SetOwnedWeapons(0b101);	 // pistol, shotgun
	Command({.cycle = 1});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun) << "past the MP5 not carried";
	Command({.cycle = 1});
	EXPECT_EQ(player_.HeldWeapon(), kPistol) << "round to the first";
	Command({.cycle = -1});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun);
}

// A weapon taken in hand comes up first: the end of its reload plays, and
// only then does it fire
TEST_F(ArsenalTest, ASwitchedWeaponComesUpBeforeItFires) {
	player_.SetOwnedWeapons(0b011);	 // pistol, MP5
	Command({.weapon = kMp5});
	Weapon& mp5 = player_.GetWeapon(kMp5);
	const auto reload =
		LoopedAnimation::Clip(testing::TestTextures(), "mp5", "reload");
	EXPECT_EQ(mp5.GetTextureId(), reload.back()) << "the end of the reload";
	const std::size_t rounds = mp5.GetAmmo();
	EXPECT_FALSE(mp5.Attack()) << "still coming up";
	EXPECT_EQ(mp5.GetAmmo(), rounds);
	WaitForTheWeapon();
	EXPECT_TRUE(mp5.Attack());
}

// A weapon whose art has its own raise clip comes up with it
TEST_F(ArsenalTest, ARaiseClipIsPreferred) {
	player_.SetOwnedWeapons(0b101);	 // pistol, shotgun
	Command({.weapon = kShotgun});
	EXPECT_EQ(
		player_.GetWeapon().GetTextureId(),
		testing::TestTextures().FindTextureCollection("shotgun_raise").front());
}

// Coming up is not a reload: no rounds move, and a reload asked for meanwhile
// is not taken
TEST_F(ArsenalTest, ComingUpIsNotAReload) {
	player_.SetOwnedWeapons(0b011);
	Weapon& mp5 = player_.GetWeapon(kMp5);
	mp5.SetRounds(5, 20);
	Command({.reload = true, .weapon = kMp5});
	WaitForTheWeapon();
	EXPECT_EQ(mp5.GetAmmo(), 5u);
	EXPECT_EQ(mp5.GetReserve(), 20u);
}

TEST_F(ArsenalTest, AnEmptyWeaponComesUpEmpty) {
	player_.SetOwnedWeapons(0b011);
	Weapon& mp5 = player_.GetWeapon(kMp5);
	mp5.SetRounds(0, 0);
	Command({.weapon = kMp5});
	WaitForTheWeapon();
	EXPECT_FALSE(mp5.Attack());
	EXPECT_EQ(mp5.GetTextureId(),
			  LoopedAnimation::Clip(testing::TestTextures(), "mp5", "outofammo")
				  .front());
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
	player_.SetOwnedWeapons(0b011);	 // pistol, MP5
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
}

// A player carrying a blade (slot 0) and the pistol (slot 1)
class MeleeTest : public ArsenalTest
{
  protected:
	static constexpr std::size_t kBlade = 0;

	MeleeTest() { scene_.SetPlayer(swordsman_); }

	std::array<WeaponConfig, 2> arsenal_{testing::Blade(),
										 testing::Weapon("pistol")};
	Player swordsman_{config_, arsenal_, kBlade, testing::TestTextures(),
					  testing::TestSound()};
};

// A blade never runs out and never reloads
TEST_F(MeleeTest, ABladeNeedsNoAmmunition) {
	const Weapon& blade = swordsman_.GetWeapon();
	ASSERT_TRUE(blade.IsMelee());
	// Stabbing and trying to reload for ten seconds
	for (int tick = 0; tick < 600; ++tick) {
		swordsman_.SetCommand({.fire = true, .reload = tick % 50 == 0});
		scene_.Update(kTick);
	}
	EXPECT_EQ(blade.GetAmmo(), 0u);
	EXPECT_EQ(blade.GetReserve(), 0u);
	// Once the last stab is over it stabs again: never out of ammunition,
	// never reloading
	swordsman_.SetCommand({});
	scene_.Update(1.0);
	EXPECT_TRUE(swordsman_.GetWeapon(kBlade).Attack());
}

// A blade reaches the enemy in front of it and no further
TEST_F(MeleeTest, ABladeReachesOnlyTheEnemyInFront) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({1.5, 4.5}, -kFacingDown)));
	scene_.FinishLoading();
	Enemy& enemy = *scene_.GetEnemies().front();
	const Weapon& blade = swordsman_.GetWeapon(kBlade);
	const double health = enemy.GetHealth();
	ResolvePlayerShot(scene_, blade, Position2D({1.5, 1.5}, kFacingDown));
	EXPECT_DOUBLE_EQ(enemy.GetHealth(), health) << "three cells away";
	ResolvePlayerShot(scene_, blade, Position2D({1.5, 3.6}, kFacingDown));
	EXPECT_LT(enemy.GetHealth(), health) << "within reach";
}

// A blade marks no wall and kicks nothing
TEST_F(MeleeTest, ABladeLeavesNoMarkAndNoKick) {
	scene_.FinishLoading();
	swordsman_.SetCommand({.fire = true});
	scene_.Update(kTick);
	EXPECT_TRUE(scene_.GetWallMarks().empty());
	EXPECT_EQ(swordsman_.GetKick(), 0.0);
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
TEST_F(ArsenalTest, SwitchingWeaponsAllocatesNothing) {
	player_.SetOwnedWeapons(0b111);
	const auto before = AllocationStats::count;
	for (int step = 0; step < 40; ++step) {
		Command({.weapon = static_cast<std::int8_t>(step % 3),
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
