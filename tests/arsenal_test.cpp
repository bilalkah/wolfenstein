// The player carries every weapon found, each with its own rounds, and
// switches between them; a melee weapon (the saw; a silent test blade here)
// needs no ammunition but reaches only the enemy in front of it. Each sounds
// its own: the double-barrelled shotgun broken open to load, the saw
// starting up and biting.

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

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y
constexpr std::size_t kPistol = 0, kMp5 = 1, kShotgun = 2, kSuperShotgun = 3,
					  kChainsaw = 4;

// How many more times `effect` is played while `action` runs
template <typename Action>
std::uint32_t Plays(SoundEffect effect, Action action) {
	const std::uint32_t before = testing::TestSound().PlayCount(effect);
	action();
	return testing::TestSound().PlayCount(effect) - before;
}

// A corridor with the player at (1.5, 1.5) facing down it, carrying the
// game's arsenal
class ArsenalTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 1};

	ArsenalTest()
		: map_(testing::WriteMapFile("karakale_arsenal_test.txt",
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
	// Until the gun in hand is down and the one asked for is in hand
	// (coming up)
	void WaitUntilInHand() {
		player_.SetCommand({});
		for (int tick = 0; tick < 120 && player_.ComingWeapon(); ++tick) {
			scene_.Update(kTick);
		}
	}
	// Asks for a weapon and waits until it is in hand
	void Switch(PlayerCommand command) {
		Command(command);
		WaitUntilInHand();
	}
	// Until a weapon asked for is in hand and up
	void WaitForTheWeapon() {
		WaitUntilInHand();
		for (int tick = 0; tick < 60; ++tick) {
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
	EXPECT_EQ(player_.WeaponCount(), 7u);
	EXPECT_TRUE(player_.Owns(kPistol));
	EXPECT_FALSE(player_.Owns(kMp5));
	EXPECT_FALSE(player_.Owns(kShotgun));
	EXPECT_EQ(player_.HeldWeapon(), kPistol);
	EXPECT_EQ(player_.GetWeapon().GetWeaponName(), "pistol");
}

TEST_F(ArsenalTest, OnlyWeaponsCarriedCanBeTakenInHand) {
	player_.SetOwnedWeapons(0b101);	 // pistol, shotgun
	Switch({.weapon = kShotgun});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun);
	Command({.weapon = kMp5});	// not carried
	EXPECT_EQ(player_.HeldWeapon(), kShotgun);
}

// The wheel steps through the weapons carried, skipping the rest, round
TEST_F(ArsenalTest, TheWheelStepsThroughWeaponsCarried) {
	player_.SetOwnedWeapons(0b101);	 // pistol, shotgun
	Switch({.cycle = 1});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun) << "past the MP5 not carried";
	Switch({.cycle = 1});
	EXPECT_EQ(player_.HeldWeapon(), kPistol) << "round to the first";
	Switch({.cycle = -1});
	EXPECT_EQ(player_.HeldWeapon(), kShotgun);
}

// A weapon taken in hand comes up first: the end of its reload plays, and
// only then does it fire
TEST_F(ArsenalTest, ASwitchedWeaponComesUpBeforeItFires) {
	player_.SetOwnedWeapons(0b011);	 // pistol, MP5
	Switch({.weapon = kMp5});
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
	Switch({.weapon = kShotgun});
	EXPECT_EQ(
		player_.GetWeapon().GetTextureId(),
		testing::TestTextures().FindTextureCollection("shotgun_raise").front());
}

// Each weapon takes its own time coming up: the shotgun longer than the rest
TEST_F(ArsenalTest, EachWeaponComesUpInItsOwnTime) {
	const double shotgun = testing::Weapon("shotgun").raise_seconds;
	ASSERT_GT(shotgun, testing::Weapon("pistol").raise_seconds);
	player_.SetOwnedWeapons(0b101);	 // pistol, shotgun
	Switch({.weapon = kShotgun});
	Weapon& weapon = player_.GetWeapon(kShotgun);
	player_.SetCommand({});
	const int almost = static_cast<int>(shotgun / kTick) - 2;
	for (int tick = 1; tick < almost; ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_FALSE(weapon.Attack()) << "still coming up";
	for (int tick = 0; tick < 4; ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_TRUE(weapon.Attack());
}

// Another weapon asked for, the one in hand goes down first (its lower
// clip), neither firing nor reloading, and only then does the other come up
TEST_F(ArsenalTest, TheGunInHandGoesDownFirst) {
	player_.SetOwnedWeapons(0b011);	 // pistol, MP5
	Command({.weapon = kMp5});
	EXPECT_EQ(player_.HeldWeapon(), kPistol);
	EXPECT_EQ(player_.ComingWeapon(), kMp5);
	Weapon& pistol = player_.GetWeapon(kPistol);
	EXPECT_FALSE(pistol.Attack()) << "going down";
	WaitUntilInHand();
	EXPECT_EQ(player_.HeldWeapon(), kMp5);
	EXPECT_FALSE(player_.ComingWeapon());
}

// Asked for again while it goes down, the gun in hand comes back up
TEST_F(ArsenalTest, ChangingYourMindBringsItBack) {
	player_.SetOwnedWeapons(0b011);
	Command({.weapon = kMp5});
	ASSERT_EQ(player_.ComingWeapon(), kMp5);
	Command({.weapon = kPistol});
	EXPECT_FALSE(player_.ComingWeapon());
	WaitForTheWeapon();
	EXPECT_EQ(player_.HeldWeapon(), kPistol);
	EXPECT_TRUE(player_.GetWeapon(kPistol).Attack());
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
	Switch({.weapon = kMp5});
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
	EXPECT_EQ(player_.ComingWeapon(), kMp5) << "the pistol goes down first";
	WaitUntilInHand();
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

// Both barrels fired, the double-barrelled shotgun is broken open and
// loaded again at once, with the sound of it: no reload key needed. With no
// shells left it stays empty.
TEST_F(ArsenalTest, TheDoubleBarrelLoadsAgainAfterItsBlast) {
	scene_.FinishLoading();
	player_.SetOwnedWeapons(0b11111);
	Switch({.weapon = kSuperShotgun});
	WaitForTheWeapon();
	Weapon& both = player_.GetWeapon(kSuperShotgun);
	ASSERT_EQ(both.GetAmmo(), 1u);
	const std::size_t reserve = both.GetReserve();
	const double cycle = both.GetAttackSpeed() + both.GetReloadSpeed() + 0.1;
	const std::uint32_t reloads = Plays(SoundEffect::SuperShotgunReload, [&] {
		Command({.fire = true});
		EXPECT_EQ(both.GetAmmo(), 0u);
		for (double t = 0.0; t < cycle; t += kTick) {
			Command({});
		}
	});
	EXPECT_EQ(reloads, 1u);
	EXPECT_EQ(both.GetAmmo(), 1u) << "loaded again";
	EXPECT_EQ(both.GetReserve(), reserve - 1);

	both.SetRounds(1, 0);
	Command({.fire = true});
	for (double t = 0.0; t < cycle; t += kTick) {
		Command({});
	}
	EXPECT_EQ(both.GetAmmo(), 0u) << "no shells to load";
}

// Held down, the trigger keeps a weapon on its firing frames from one
// stroke to the next: the saw cuts on without jumping back to its rest
// between strokes, and is back at rest once let go
TEST_F(ArsenalTest, HeldDownItStaysOnItsFiringFrames) {
	scene_.FinishLoading();
	player_.SetOwnedWeapons(0b11111);
	Switch({.weapon = kChainsaw});
	WaitForTheWeapon();
	const Weapon& saw = player_.GetWeapon();
	const int rest = saw.GetTextureId();
	EXPECT_EQ(rest, LoopedAnimation::Clip(testing::TestTextures(), "chainsaw",
										  "loaded")
						.front());
	for (int tick = 0; tick < 60; ++tick) {
		Command({.fire = true});
		EXPECT_NE(saw.GetTextureId(), rest) << tick;
	}
	for (int tick = 0; tick < 30; ++tick) {
		Command({});
	}
	EXPECT_EQ(saw.GetTextureId(), rest);
}

// The saw starts up as it comes up, cuts the air with one sound and an
// enemy with another
TEST_F(ArsenalTest, TheSawSoundsWhatItCuts) {
	ASSERT_TRUE(scene_.AddEnemy(testing::Enemy("soldier"),
								Position2D({1.5, 5.5}, -kFacingDown)));
	scene_.FinishLoading();
	Enemy& enemy = *scene_.GetEnemies().front();
	player_.SetOwnedWeapons(0b11111);
	EXPECT_EQ(Plays(SoundEffect::SawUp,
					[&] {
						Switch({.weapon = kChainsaw});
						WaitForTheWeapon();
					}),
			  1u);
	ASSERT_TRUE(player_.GetWeapon().IsMelee());
	// The enemy four cells off: the saw cuts air
	EXPECT_GE(Plays(SoundEffect::Saw, [&] { Command({.fire = true}); }), 1u);
	// Within reach, it bites
	enemy.SetPose({1.5, 2.5});
	const double health = enemy.GetHealth();
	const std::uint32_t bites = Plays(SoundEffect::SawHit, [&] {
		for (int tick = 0; tick < 30; ++tick) {
			Command({.fire = true});
		}
	});
	EXPECT_GE(bites, 1u);
	EXPECT_LT(enemy.GetHealth(), health);
}

#ifdef KARAKALE_COUNTS_ALLOCATIONS
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
	for (int tick = 0; tick < 60 && player.ComingWeapon(); ++tick) {
		world.CurrentLevel().Update(kTick);
	}
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
}  // namespace karakale
