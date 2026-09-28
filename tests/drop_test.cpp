// Enemies carry something (a soldier a clip) and drop it where they die.
// Each drop is a pickup made with the level, hidden until then, so a death
// allocates nothing.

#include "Core/world.h"
#include "test_services.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

namespace wolfenstein {
namespace {

// The first level: seven soldiers, each with a clip
class DropTest : public ::testing::Test
{
  protected:
	void SetUp() override {
		auto loader = SceneLoader::Open(RESOURCE_DIR);
		ASSERT_TRUE(loader) << loader.error();
		world_ =
			std::make_unique<World>(testing::TestTextures(), std::move(*loader),
									std::make_unique<SoundManager>());
		ASSERT_TRUE(world_->NewGame({}));
		ASSERT_EQ(testing::Enemy("soldier").drop, "clip");
	}

	Scene& Level() { return world_->CurrentLevel(); }
	// The drop each soldier carries: after the level's own pickups, in the
	// soldiers' order
	Pickup& DropOf(std::size_t enemy) {
		const auto pickups = Level().GetPickups();
		const auto own = static_cast<std::size_t>(std::ranges::count_if(
			pickups, [](const Pickup* pickup) { return !pickup->IsDrop(); }));
		return *pickups[own + enemy];
	}
	void Kill(std::size_t enemy) { Level().RestoreKilled(enemy); }

	std::unique_ptr<World> world_;
};

TEST_F(DropTest, EachSoldierCarriesAHiddenClip) {
	const auto enemies = Level().GetEnemies();
	for (std::size_t i = 0; i < enemies.size(); ++i) {
		EXPECT_TRUE(DropOf(i).IsDrop());
		EXPECT_TRUE(DropOf(i).IsTaken()) << "hidden until it drops";
		EXPECT_FALSE(DropOf(i).IsVisible());
	}
}

TEST_F(DropTest, ASoldierDropsItsClipWhereItDies) {
	Enemy& enemy = *Level().GetEnemies()[2];
	Kill(2);
	ASSERT_FALSE(enemy.IsAlive());
	EXPECT_FALSE(DropOf(2).IsTaken());
	EXPECT_TRUE(DropOf(2).IsVisible());
	EXPECT_EQ(DropOf(2).GetPose(), enemy.GetPose());
	EXPECT_TRUE(DropOf(3).IsTaken()) << "the others still carry theirs";
}

// The level's supplies are what it lays out; what enemies drop is extra
TEST_F(DropTest, DropsAreNotCountedAsTheLevelsSupplies) {
	const std::size_t supplies = Level().GetStats().pickups;
	const auto own = static_cast<std::size_t>(std::ranges::count_if(
		Level().GetPickups(),
		[](const Pickup* pickup) { return !pickup->IsDrop(); }));
	EXPECT_EQ(supplies, own);
	Kill(0);
	EXPECT_EQ(Level().GetStats().pickups, supplies);
}

// A clip is half a box of rounds, for every gun carried
TEST_F(DropTest, AClipIsHalfABox) {
	Player& player = world_->GetPlayer();
	Weapon& pistol = player.GetWeapon(0);
	pistol.SetRounds(pistol.GetAmmo(), 0);
	ASSERT_TRUE(player.TryPickUp(DropOf(0).GetEffect()));
	EXPECT_EQ(pistol.GetReserve(), testing::Weapon("pistol").box_rounds / 2);
}

// A saved game keeps each drop as it was: lying there, taken, or carried
TEST_F(DropTest, ASavedGameKeepsTheDrops) {
	Kill(0);
	Kill(1);
	DropOf(1).Take();  // picked up
	const auto saved = world_->Capture();
	ASSERT_TRUE(saved);

	ASSERT_TRUE(world_->ContinueGame(saved.value_or(SavedGame{})));
	EXPECT_TRUE(DropOf(0).IsVisible()) << "dropped, lying there";
	EXPECT_FALSE(DropOf(1).IsVisible()) << "taken";
	EXPECT_FALSE(DropOf(2).IsVisible()) << "still carried";
	Kill(2);
	EXPECT_TRUE(DropOf(2).IsVisible()) << "and dropped when it dies";
}

}  // namespace
}  // namespace wolfenstein
