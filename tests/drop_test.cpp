// Enemies may carry something (a soldier a clip, or a medkit) and drop it
// where they die. Each kind has its own chances, rolled from the game's seed
// as a level is made, likelier the more supplies the difficulty gives. Each
// drop is a pickup made with the level, hidden until then, so a death
// allocates nothing; everything an enemy may drop is made, carried or not,
// so a saved game finds the same pickups whatever was rolled.

#include "Core/world.h"
#include "test_services.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <string_view>

namespace wolfenstein {
namespace {

// The first level: seven soldiers, each may carry a clip and a medkit
class DropTest : public ::testing::Test
{
  protected:
	static constexpr std::size_t kClip = 0, kMedkit = 1;

	void SetUp() override {
		auto loader = SceneLoader::Open(RESOURCE_DIR);
		ASSERT_TRUE(loader) << loader.error();
		world_ =
			std::make_unique<World>(testing::TestTextures(), std::move(*loader),
									std::make_unique<SoundManager>());
		Start(1);
		const auto& drops = testing::Enemy("soldier").drops;
		ASSERT_EQ(drops.size(), 2u);
		ASSERT_EQ(drops[kClip].pickup, "clip");
		ASSERT_EQ(drops[kMedkit].pickup, "medkit");
	}
	void Start(std::uint64_t seed, std::string_view difficulty = "normal") {
		ASSERT_TRUE(world_->NewGame({}, {}, difficulty, seed));
	}

	Scene& Level() { return world_->CurrentLevel(); }
	// The pickups made for enemy `enemy` to drop, one for each thing its
	// kind may drop: after the level's own pickups, in the enemies' order
	std::span<Pickup* const> MadeFor(std::size_t enemy) {
		const auto pickups = Level().GetPickups();
		auto at = static_cast<std::size_t>(std::ranges::count_if(
			pickups, [](const Pickup* pickup) { return !pickup->IsDrop(); }));
		const auto enemies = Level().GetEnemies();
		const auto count = [&](std::size_t i) {
			return testing::Enemy(enemies[i]->GetBotName()).drops.size();
		};
		for (std::size_t i = 0; i < enemy; ++i) {
			at += count(i);
		}
		return pickups.subspan(at, count(enemy));
	}
	// Whether enemy `enemy` carries the `entry`th thing its kind may drop
	bool Carries(std::size_t enemy, std::size_t entry) {
		const auto carried = Level().GetEnemies()[enemy]->GetDrops();
		return std::ranges::find(carried, MadeFor(enemy)[entry]) !=
			   carried.end();
	}
	// How many of the level's soldiers carry the `entry`th thing, in the
	// games from seed 1 to `games`
	double ShareCarrying(std::size_t entry, int games,
						 std::string_view difficulty = "normal") {
		int carrying = 0;
		int soldiers = 0;
		for (int seed = 1; seed <= games; ++seed) {
			Start(static_cast<std::uint64_t>(seed), difficulty);
			for (std::size_t i = 0; i < Level().GetEnemies().size(); ++i) {
				carrying += Carries(i, entry) ? 1 : 0;
				++soldiers;
			}
		}
		return static_cast<double>(carrying) / soldiers;
	}
	void Kill(std::size_t enemy) { Level().RestoreKilled(enemy); }

	std::unique_ptr<World> world_;
};

// Everything an enemy may drop is made, hidden; it carries some of it
TEST_F(DropTest, EverythingItMayDropIsMadeHidden) {
	const auto enemies = Level().GetEnemies();
	for (std::size_t i = 0; i < enemies.size(); ++i) {
		ASSERT_EQ(MadeFor(i).size(), 2u);
		for (const Pickup* made : MadeFor(i)) {
			EXPECT_TRUE(made->IsDrop());
			EXPECT_FALSE(made->IsVisible()) << "hidden until dropped";
		}
		for (const Pickup* carried : enemies[i]->GetDrops()) {
			EXPECT_NE(std::ranges::find(MadeFor(i), carried), MadeFor(i).end());
		}
	}
}

// It drops what it carries where it dies, and only that
TEST_F(DropTest, ItDropsWhatItCarriesWhereItDies) {
	const auto enemies = Level().GetEnemies();
	const auto carrier = std::ranges::find_if(
		enemies, [](const Enemy* enemy) { return !enemy->GetDrops().empty(); });
	ASSERT_NE(carrier, enemies.end());
	const auto index = static_cast<std::size_t>(carrier - enemies.begin());
	const std::array<bool, 2> carried{Carries(index, kClip),
									  Carries(index, kMedkit)};
	Kill(index);
	ASSERT_FALSE((*carrier)->IsAlive());
	EXPECT_TRUE((*carrier)->GetDrops().empty()) << "all of it dropped";
	for (std::size_t entry = 0; entry < MadeFor(index).size(); ++entry) {
		const Pickup& made = *MadeFor(index)[entry];
		EXPECT_EQ(made.IsVisible(), carried[entry]) << entry;
		if (made.IsVisible()) {
			EXPECT_LT(made.GetPose().Distance((*carrier)->GetPose()), 0.5);
		}
	}
	for (std::size_t other = 0; other < enemies.size(); ++other) {
		if (other != index) {
			for (const Pickup* made : MadeFor(other)) {
				EXPECT_FALSE(made->IsVisible()) << "still carried";
			}
		}
	}
}

// Two things dropped lie side by side, not one on the other
TEST_F(DropTest, TwoDropsLieSideBySide) {
	for (std::uint64_t seed = 1; seed < 200; ++seed) {
		Start(seed);
		const auto enemies = Level().GetEnemies();
		for (std::size_t i = 0; i < enemies.size(); ++i) {
			if (enemies[i]->GetDrops().size() < 2) {
				continue;
			}
			Kill(i);
			const Pickup& clip = *MadeFor(i)[kClip];
			const Pickup& medkit = *MadeFor(i)[kMedkit];
			ASSERT_TRUE(clip.IsVisible() && medkit.IsVisible());
			EXPECT_GE(clip.GetPose().Distance(medkit.GetPose()), 0.25);
			EXPECT_FALSE(Level().GetMap().IsBlocked(clip.GetPose()));
			EXPECT_FALSE(Level().GetMap().IsBlocked(medkit.GetPose()));
			return;
		}
	}
	FAIL() << "no soldier carried both in 200 games";
}

// Over many games each is carried as often as its chance says
TEST_F(DropTest, TheChancesHold) {
	constexpr int kGames = 300;
	EXPECT_NEAR(ShareCarrying(kClip, kGames),
				testing::Enemy("soldier").drops[kClip].chance, 0.04);
	EXPECT_NEAR(ShareCarrying(kMedkit, kGames),
				testing::Enemy("soldier").drops[kMedkit].chance, 0.03);
}

// The difficulty's supplies scale the chances: an easy game's soldiers
// always carry a clip, a hard game's less often
TEST_F(DropTest, SuppliesScaleTheChances) {
	constexpr int kGames = 300;
	EXPECT_DOUBLE_EQ(ShareCarrying(kClip, 20, "easy"), 1.0);
	const double hard = testing::GameData().FindDifficulty("hard")->supplies;
	EXPECT_NEAR(ShareCarrying(kClip, kGames, "hard"),
				hard * testing::Enemy("soldier").drops[kClip].chance, 0.04);
}

// The same game (its seed) drops the same, every time its level is made;
// another game drops otherwise
TEST_F(DropTest, TheSameGameDropsTheSame) {
	const auto pattern = [&] {
		std::uint32_t bits = 0;
		for (std::size_t i = 0; i < Level().GetEnemies().size(); ++i) {
			for (std::size_t entry = 0; entry < 2; ++entry) {
				bits = bits << 1 | (Carries(i, entry) ? 1U : 0U);
			}
		}
		return bits;
	};
	Start(42);
	const std::uint32_t first = pattern();
	Start(43);
	Start(42);
	EXPECT_EQ(pattern(), first);
	bool other = false;
	for (std::uint64_t seed = 43; seed < 50 && !other; ++seed) {
		Start(seed);
		other = pattern() != first;
	}
	EXPECT_TRUE(other);
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
	ASSERT_TRUE(player.TryPickUp(MadeFor(0)[kClip]->GetEffect()));
	EXPECT_EQ(pistol.GetReserve(), testing::Weapon("pistol").box_rounds / 2);
}

// A saved game keeps its seed, so its enemies carry what they did, and
// each drop as it was: lying there, taken, or carried
TEST_F(DropTest, ASavedGameKeepsTheDrops) {
	// A game in which the first two soldiers carry a clip
	std::uint64_t seed = 1;
	while (!(Carries(0, kClip) && Carries(1, kClip))) {
		ASSERT_LT(++seed, 100u);
		Start(seed);
	}
	Kill(0);
	Kill(1);
	MadeFor(1)[kClip]->Take();	// picked up
	const auto saved = world_->Capture();
	ASSERT_TRUE(saved);
	EXPECT_EQ(saved.value_or(SavedGame{}).seed, seed);

	Start(seed + 7);  // another game meanwhile
	ASSERT_TRUE(world_->ContinueGame(saved.value_or(SavedGame{})));
	EXPECT_TRUE(MadeFor(0)[kClip]->IsVisible()) << "dropped, lying there";
	EXPECT_FALSE(MadeFor(1)[kClip]->IsVisible()) << "taken";
	for (std::size_t i = 2; i < Level().GetEnemies().size(); ++i) {
		for (const Pickup* made : MadeFor(i)) {
			EXPECT_FALSE(made->IsVisible()) << "still carried";
		}
	}
}

}  // namespace
}  // namespace wolfenstein
