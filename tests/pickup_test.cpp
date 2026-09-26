// Pickups lie in a level until the player walks over one it has a use for;
// weapons carry a limited reserve of rounds that reloads draw from

#include "Camera/camera.h"
#include "Core/scene.h"
#include "Profiler/profiler.h"
#include "test_map.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kFacingDown = std::numbers::pi / 2;  // towards +y

// The player in a corridor, standing at (1.5, 1.5)
class PickupTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.pickups = 2};

	PickupTest()
		: map_(testing::WriteMapFile("wolfenstein_pickup_test.txt",
									 {"3333333", "3000003", "3333333"})
				   .string()),
		  arena_(Scene::MemoryFor(map_, kCapacity)),
		  scene_(testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_),
		  player_(config_, testing::Weapon("mp5"), testing::TestTextures(),
				  testing::TestSound()) {
		scene_.SetPlayer(player_);
	}

	Pickup& Place(std::string_view type, vector2d where) {
		const PickupConfig& pickup =
			testing::GameData().pickups.find(type)->second;
		const auto handle = scene_.AddPickup(
			where, testing::TestTextures().GetTextureId(pickup.texture),
			pickup.width, pickup.height, pickup.effect);
		EXPECT_TRUE(handle);
		return *scene_.GetPickups().back();
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kFacingDown), 2.0, 0.4, 0.4,
							1.0};
	Map map_;
	memory::MonotonicArena arena_;
	Scene scene_;
	Player player_;
};

TEST_F(PickupTest, AMedkitHealsAndLeavesTheLevel) {
	Pickup& medkit = Place("medkit", {1.5, 1.5});
	player_.DecreaseHealth(40);

	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetHealth(), 85.0);
	EXPECT_TRUE(medkit.IsTaken());
	EXPECT_FALSE(medkit.IsVisible());
	EXPECT_GT(player_.GetPickupAlpha(), 0);

	// Taken once: it gives nothing more
	player_.DecreaseHealth(40);
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetHealth(), 45.0);
}

TEST_F(PickupTest, HealthNeverGoesPastFull) {
	Place("large_medkit", {1.5, 1.5});
	player_.DecreaseHealth(10);
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetHealth(), 100.0);
}

// On an easy game pickups give more
TEST_F(PickupTest, TheDifficultyScalesWhatPickupsGive) {
	scene_.SetDifficulty({.supplies = 1.5});
	Place("medkit", {1.5, 1.5});
	Place("ammo_box", {1.5, 1.5});
	player_.DecreaseHealth(50);
	scene_.Update(kTick);
	EXPECT_DOUBLE_EQ(player_.GetHealth(), 50.0 + 25.0 * 1.5);
	const WeaponConfig& mp5 = testing::Weapon("mp5");
	EXPECT_EQ(player_.GetWeapon().GetReserve(),
			  mp5.reserve_start + mp5.box_rounds * 3 / 2);
}

TEST_F(PickupTest, AtFullHealthAMedkitIsLeftLying) {
	Pickup& medkit = Place("medkit", {1.5, 1.5});
	scene_.Update(kTick);
	EXPECT_FALSE(medkit.IsTaken());
	EXPECT_EQ(player_.GetPickupAlpha(), 0);
}

TEST_F(PickupTest, OnlyWhatThePlayerTouchesIsTaken) {
	Pickup& medkit = Place("medkit", {1.5, 4.5});
	player_.DecreaseHealth(40);
	scene_.Update(kTick);
	EXPECT_FALSE(medkit.IsTaken());

	// Walk down the corridor onto it
	player_.SetCommand(PlayerCommand{.forward = 1});
	for (int tick = 0; tick < 120 && !medkit.IsTaken(); ++tick) {
		scene_.Update(kTick);
	}
	EXPECT_TRUE(medkit.IsTaken());
}

TEST_F(PickupTest, AmmoBoxesFillTheReserveUpToItsMost) {
	const WeaponConfig& mp5 = testing::Weapon("mp5");
	const Weapon& weapon = player_.GetWeapon();
	ASSERT_EQ(weapon.GetReserve(), mp5.reserve_start);

	Pickup& first = Place("ammo_box", {1.5, 1.5});
	scene_.Update(kTick);
	EXPECT_TRUE(first.IsTaken());
	EXPECT_EQ(weapon.GetReserve(), mp5.reserve_start + mp5.box_rounds);

	// A full reserve leaves the box lying
	while (player_.TryPickUp({.ammo_boxes = 1})) {}
	EXPECT_EQ(weapon.GetReserve(), mp5.reserve_max);
	Pickup& second = Place("ammo_box", {1.5, 1.5});
	scene_.Update(kTick);
	EXPECT_FALSE(second.IsTaken());
}

// A taken pickup stays in the scene's list but the camera no longer sees it
TEST_F(PickupTest, ATakenPickupIsNotDrawn) {
	Pickup& medkit = Place("medkit", {1.5, 3.5});
	Camera2D camera(Camera2DConfig(320, std::numbers::pi / 3, 15.0));
	camera.SetScene(scene_);
	const Position2D eye({1.5, 1.5}, kFacingDown);
	camera.Update(eye, 1.0);
	EXPECT_NE(camera.FindObjectRays(medkit.GetId()), nullptr);

	medkit.Take();
	camera.Update(eye, 1.0);
	EXPECT_EQ(camera.FindObjectRays(medkit.GetId()), nullptr);
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
TEST_F(PickupTest, CollectingAllocatesNothing) {
	Place("medkit", {1.5, 1.5});
	Place("ammo_box", {1.5, 1.5});
	player_.DecreaseHealth(40);
	const auto before = AllocationStats::count;
	scene_.Update(kTick);
	EXPECT_EQ(AllocationStats::count - before, 0u);
	EXPECT_EQ(scene_.GetPickups()[0]->IsTaken(), true);
	EXPECT_EQ(scene_.GetPickups()[1]->IsTaken(), true);
}
#endif

// Reloading moves rounds from the reserve into the magazine
class ReserveTest : public ::testing::Test
{
  protected:
	void Empty(std::size_t rounds) {
		for (std::size_t i = 0; i < rounds; ++i) {
			weapon_.DecreaseAmmo();
		}
	}
	void FinishReloading() {
		weapon_.Reload();
		weapon_.Update(testing::Weapon("mp5").reload_speed + kTick);
	}

	Weapon weapon_{testing::Weapon("mp5"), testing::TestTextures(),
				   testing::TestSound()};
};

TEST_F(ReserveTest, AReloadFillsTheMagazineFromTheReserve) {
	Empty(18);
	FinishReloading();
	EXPECT_EQ(weapon_.GetAmmo(), 18u);
	EXPECT_EQ(weapon_.GetReserve(), 54u - 18u);

	// Only the rounds missing are moved
	Empty(5);
	FinishReloading();
	EXPECT_EQ(weapon_.GetAmmo(), 18u);
	EXPECT_EQ(weapon_.GetReserve(), 54u - 23u);
}

TEST_F(ReserveTest, AnEmptyReserveCannotReload) {
	for (int reload = 0; reload < 3; ++reload) {  // spend the 54 in reserve
		Empty(18);
		FinishReloading();
	}
	ASSERT_EQ(weapon_.GetReserve(), 0u);
	ASSERT_TRUE(weapon_.AddAmmoBoxes(1));  // 36 more
	Empty(18);
	FinishReloading();
	Empty(18);
	FinishReloading();
	EXPECT_EQ(weapon_.GetReserve(), 0u);
	Empty(18);
	// Nothing left to reload with: the magazine stays empty
	FinishReloading();
	EXPECT_EQ(weapon_.GetAmmo(), 0u);
}

TEST_F(ReserveTest, AFullMagazineIsNotReloaded) {
	FinishReloading();
	EXPECT_EQ(weapon_.GetAmmo(), 18u);
	EXPECT_EQ(weapon_.GetReserve(), 54u);
}

}  // namespace
}  // namespace wolfenstein
