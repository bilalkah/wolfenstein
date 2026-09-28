// How enemies fight: each kind at its own range from the player, closing in
// from further off and backing away, its gun on the player, from nearer;
// between shots, stepping aside; badly hurt, running for cover; in a group,
// spreading out round the player, not filing up to them, and taking turns
// to shoot

#include "Core/scene.h"
#include "NavigationManager/navigation_manager.h"
#include "test_map.h"
#include "test_services.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <string>
#include <utility>

namespace wolfenstein {
namespace {

constexpr double kTick = 1.0 / 60.0;
constexpr double kDown = std::numbers::pi / 2;	// along +y

// A walled hall, open from (1, 1) to (12, 16); a map file's rows run along x
constexpr std::initializer_list<const char*> kHall = {
	"333333333333333333", "300000000000000003", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "333333333333333333"};

// The same hall with a block of wall in it, 4 cells across (x 5 to 8) and 2
// deep (y 9 and 10): cover from a player at its near side
constexpr std::initializer_list<const char*> kBlock = {
	"333333333333333333", "300000000000000003", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000330000003",
	"300000000330000003", "300000000330000003", "300000000330000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "333333333333333333"};

// The hall split across by a wall at x = 7, with two gaps in it, at y = 4
// and at y = 13
constexpr std::initializer_list<const char*> kTwoGaps = {
	"333333333333333333", "300000000000000003", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "333303333333303333", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "333333333333333333"};

// The hall split across by a wall at x = 7 with a door in it at y = 8
constexpr std::initializer_list<const char*> kDoorway = {
	"333333333333333333", "300000000000000003", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "33333333D333333333", "300000000000000003",
	"300000000000000003", "300000000000000003", "300000000000000003",
	"300000000000000003", "333333333333333333"};

class TacticsTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{.enemies = 6};

	// The player at `player`, looking along +y, and enemies of the given
	// kinds, each looking back at them
	void Load(vector2d player,
			  std::initializer_list<std::pair<const char*, vector2d>> enemies,
			  std::initializer_list<const char*> rows = kHall) {
		config_.initial_position = Position2D(player, kDown);
		player_ = std::make_unique<Player>(config_, testing::GameData().weapons,
										   0, testing::TestTextures(),
										   testing::TestSound());
		map_ = std::make_unique<Map>(
			testing::WriteMapFile("wolfenstein_tactics_test.txt", rows)
				.string());
		arena_ = std::make_unique<memory::MonotonicArena>(
			Scene::MemoryFor(*map_, kCapacity));
		scene_ = std::make_unique<Scene>(testing::TestTextures(),
										 testing::TestSound(), *map_, kCapacity,
										 *arena_);
		scene_->SetPlayer(*player_);
		scene_->SetDifficulty(difficulty_);
		for (const auto& [type, at] : enemies) {
			ASSERT_TRUE(
				scene_->AddEnemy(testing::Enemy(type), Position2D(at, -kDown)));
		}
		scene_->FinishLoading();
	}
	Enemy& First() { return *scene_->GetEnemies().front(); }
	double Distance(const Enemy& enemy) const {
		return enemy.GetPose().Distance(player_->GetPose());
	}
	// Runs until `done` (at most `seconds`); true if it came to that
	template <typename Done>
	bool RunUntil(double seconds, Done done) {
		for (double t = 0.0; t < seconds; t += kTick) {
			scene_->Update(kTick);
			if (done()) {
				return true;
			}
		}
		return false;
	}
	void Run(double seconds) {
		RunUntil(seconds, [] { return false; });
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, kDown), 2.0, 0.4, 0.4, 1.0};
	Difficulty difficulty_;
	std::unique_ptr<Player> player_;
	std::unique_ptr<Map> map_;
	std::unique_ptr<memory::MonotonicArena> arena_;
	std::unique_ptr<Scene> scene_;
};

// A soldier closes in on a player it sees, and then keeps its distance: in
// rifle range, not at arm's length
TEST_F(TacticsTest, ASoldierFightsFromItsRange) {
	Load({6.5, 2.5}, {{"soldier", {6.5, 9.0}}});
	const StateConfig& tactics = First().GetStateConfig();
	Run(4.0);
	for (double t = 0.0; t < 4.0; t += kTick) {
		scene_->Update(kTick);
		EXPECT_LE(Distance(First()), tactics.far_range + 0.3) << t;
		EXPECT_GE(Distance(First()), tactics.near_range - 0.3) << t;
	}
}

// Rushed, it backs away, keeping its gun on the player
TEST_F(TacticsTest, RushedItBacksAwayFacingThePlayer) {
	Load({6.5, 7.5}, {{"soldier", {6.5, 8.5}}});
	Enemy& soldier = First();
	int backing = 0;
	const bool away = RunUntil(8.0, [&] {
		if (soldier.IsMoving()) {
			++backing;
			const vector2d to = player_->GetPose() - soldier.GetPose();
			EXPECT_GT(
				std::cos(std::atan2(to.y, to.x) - soldier.GetPosition().theta),
				0.9)
				<< "facing the player";
		}
		return Distance(soldier) >= soldier.GetStateConfig().near_range;
	});
	EXPECT_TRUE(away);
	EXPECT_GT(backing, 30);
}

// With its back to a wall it backs off along it, never into it
TEST_F(TacticsTest, ItBacksAlongAWallNotIntoIt) {
	Load({6.5, 14.8}, {{"soldier", {6.5, 16.0}}});
	Enemy& soldier = First();
	for (double t = 0.0; t < 4.0; t += kTick) {
		scene_->Update(kTick);
		const vector2d at = soldier.GetPose();
		for (const vector2d off : {vector2d{soldier.GetRadius(), 0.0},
								   vector2d{-soldier.GetRadius(), 0.0},
								   vector2d{0.0, soldier.GetRadius()},
								   vector2d{0.0, -soldier.GetRadius()}}) {
			ASSERT_FALSE(map_->IsBlocked(at + off)) << t;
		}
	}
}

// A shotgun zombie comes close, where its shot hurts most
TEST_F(TacticsTest, AShotgunZombieClosesIn) {
	Load({6.5, 2.5}, {{"shotgun_zombie", {6.5, 8.0}}});
	const double far = First().GetStateConfig().far_range;
	EXPECT_TRUE(RunUntil(8.0, [&] { return Distance(First()) <= far; }));
	Run(2.0);
	EXPECT_LE(Distance(First()), far + 0.3);
}

// A demon comes right up, into its bite's reach
TEST_F(TacticsTest, ADemonComesRightUp) {
	Load({6.5, 2.5}, {{"demon", {6.5, 8.5}}});
	const double bite = First().GetWeapon().GetAttackRange();
	EXPECT_TRUE(RunUntil(6.0, [&] { return Distance(First()) < bite; }));
}

// Between shots a soldier steps aside, this way and that, the player in
// its sights all the while, and keeps its range
TEST_F(TacticsTest, BetweenShotsASoldierStepsAside) {
	Load({6.5, 2.5}, {{"soldier", {6.5, 6.0}}});
	Enemy& soldier = First();
	bool left = false;
	bool right = false;
	int shots = 0;
	bool attacking = false;
	for (double t = 0.0; t < 8.0; t += kTick) {
		const double before = soldier.GetPose().x;
		scene_->Update(kTick);
		const bool now = soldier.GetStateType() == EnemyStateType::Attack;
		shots += now && !attacking ? 1 : 0;
		attacking = now;
		// Across the line to the player, which runs along y
		const double across = soldier.GetPose().x - before;
		left = left || across > 1e-3;
		right = right || across < -1e-3;
		EXPECT_TRUE(soldier.IsPlayerInShootingRange()) << t;
		EXPECT_NEAR(soldier.GetPose().y, 6.0, 0.5) << "keeping its range";
		if (soldier.IsMoving()) {
			const vector2d to = player_->GetPose() - soldier.GetPose();
			EXPECT_GT(
				std::cos(std::atan2(to.y, to.x) - soldier.GetPosition().theta),
				0.9)
				<< t;
		}
	}
	EXPECT_GE(shots, 3);
	EXPECT_TRUE(left && right) << "both ways";
}

// The minigun zombie stands planted, firing
TEST_F(TacticsTest, TheMinigunZombieStandsItsGround) {
	Load({6.5, 2.5}, {{"minigun_zombie", {6.5, 7.0}}});
	Run(0.5);
	for (double t = 0.0; t < 4.0; t += kTick) {
		scene_->Update(kTick);
		EXPECT_FALSE(First().IsMoving()) << t;
	}
}

// By a wall it steps aside only into the open
TEST_F(TacticsTest, ItStepsAsideOnlyIntoTheOpen) {
	Load({1.6, 2.5}, {{"soldier", {1.6, 6.0}}});
	Enemy& soldier = First();
	double most = soldier.GetPose().x;
	for (double t = 0.0; t < 6.0; t += kTick) {
		scene_->Update(kTick);
		most = std::max(most, soldier.GetPose().x);
		ASSERT_FALSE(map_->IsBlocked(soldier.GetPose() -
									 vector2d{soldier.GetRadius(), 0.0}))
			<< t;
	}
	EXPECT_GT(most, 2.0) << "it stepped out from the wall";
}

// Badly hurt, a soldier breaks off and runs where the player cannot see it,
// hides there a while, and comes back to fight
TEST_F(TacticsTest, BadlyHurtASoldierRunsForCoverAndComesBack) {
	Load({6.5, 2.5}, {{"soldier", {4.5, 6.5}}}, kBlock);
	Enemy& soldier = First();
	Run(0.5);
	ASSERT_EQ(soldier.GetStateType(), EnemyStateType::Walk);
	// Hurt without a flinch: its wounds alone send it off
	soldier.DecreaseHealth(soldier.GetHealth() * 0.8);
	scene_->Update(kTick);
	EXPECT_EQ(soldier.GetStateType(), EnemyStateType::Retreat);
	EXPECT_TRUE(RunUntil(10.0, [&] {
		return soldier.GetPose().Distance(soldier.Cover()) <
			   NavigationManager::kCellSize;
	})) << "it reached its cover";
	EXPECT_FALSE(soldier.IsPlayerInShootingRange()) << "out of sight";
	EXPECT_TRUE(RunUntil(RetreatState::kHideSeconds + 0.5, [&] {
		return soldier.GetStateType() == EnemyStateType::Walk;
	})) << "it comes back";
	EXPECT_TRUE(RunUntil(6.0, [&] {
		return soldier.IsPlayerInShootingRange();
	})) << "into the player's sight, to fight";
	// Once only: hurt still, it fights to the end
	Run(3.0);
	EXPECT_NE(soldier.GetStateType(), EnemyStateType::Retreat);
}

// With nowhere out of sight to run to, it fights on
TEST_F(TacticsTest, WithNoCoverItFightsOn) {
	Load({6.5, 8.5}, {{"soldier", {6.5, 12.0}}});
	Enemy& soldier = First();
	Run(0.5);
	soldier.DecreaseHealth(soldier.GetHealth() * 0.8);
	for (double t = 0.0; t < 3.0; t += kTick) {
		scene_->Update(kTick);
		EXPECT_NE(soldier.GetStateType(), EnemyStateType::Retreat) << t;
	}
}

// A demon never breaks off
TEST_F(TacticsTest, ADemonFightsToTheEnd) {
	Load({6.5, 2.5}, {{"demon", {4.5, 6.5}}}, kBlock);
	Enemy& demon = First();
	Run(0.5);
	demon.DecreaseHealth(demon.GetHealth() * 0.9);
	for (double t = 0.0; t < 3.0; t += kTick) {
		scene_->Update(kTick);
		EXPECT_NE(demon.GetStateType(), EnemyStateType::Retreat) << t;
	}
}

// Found in its cover, it fights at once
TEST_F(TacticsTest, FoundInHidingItFights) {
	Load({6.5, 2.5}, {{"soldier", {4.5, 6.5}}}, kBlock);
	Enemy& soldier = First();
	Run(0.5);
	soldier.DecreaseHealth(soldier.GetHealth() * 0.8);
	ASSERT_TRUE(RunUntil(10.0, [&] {
		return soldier.GetPose().Distance(soldier.Cover()) <
			   NavigationManager::kCellSize;
	}));
	// The player walks round the block and sees it
	player_->SetPosition(Position2D({11.5, soldier.GetPose().y}, kDown));
	EXPECT_TRUE(RunUntil(0.5, [&] {
		return soldier.GetStateType() != EnemyStateType::Retreat;
	}));
}

// Three soldiers coming at the player from one side fan out round them
TEST_F(TacticsTest, AGroupSpreadsRoundThePlayer) {
	Load({6.5, 3.5}, {{"soldier", {5.5, 9.5}},
					  {"soldier", {6.5, 10.0}},
					  {"soldier", {7.5, 9.5}}});
	const auto bearing = [&](const Enemy* enemy) {
		const vector2d at = enemy->GetPose() - player_->GetPose();
		return std::atan2(at.y, at.x);
	};
	const auto spread = [&] {
		double least = 1e9;
		double most = -1e9;
		for (const Enemy* enemy : scene_->GetEnemies()) {
			least = std::min(least, bearing(enemy));
			most = std::max(most, bearing(enemy));
		}
		return most - least;
	};
	const double before = spread();
	EXPECT_TRUE(RunUntil(8.0, [&] { return spread() > before + 0.5; }))
		<< "from " << before << " to " << spread();
}

// Two hunting the player behind a wall with two ways through take a way
// each, not one after the other
TEST_F(TacticsTest, TheyTakeDifferentWays) {
	Load({3.5, 8.5}, {{"soldier", {9.5, 8.2}}, {"soldier", {9.5, 8.8}}},
		 kTwoGaps);
	// The way each went through the wall: its y as it crossed x = 7
	std::array<double, 2> through{-1.0, -1.0};
	int tick = 0;
	RunUntil(12.0, [&] {
		// Gunfire keeps them hunting, out of sight as the player is
		if (tick++ % 60 == 0) {
			for (Enemy* enemy : scene_->GetEnemies()) {
				enemy->Alert();
			}
		}
		for (std::size_t i = 0; i < through.size(); ++i) {
			const vector2d at = scene_->GetEnemies()[i]->GetPose();
			if (through[i] < 0.0 && at.x < 7.5) {
				through[i] = at.y;
			}
		}
		return through[0] >= 0.0 && through[1] >= 0.0;
	});
	ASSERT_GE(through[0], 0.0);
	ASSERT_GE(through[1], 0.0);
	EXPECT_NE(through[0] < 8.5, through[1] < 8.5)
		<< through[0] << " and " << through[1];
}

// Through a door, it does not stop in the doorway to fight, where it would
// block the way: it comes on into the room
TEST_F(TacticsTest, ItDoesNotStopInADoorway) {
	Load({4.5, 8.5}, {{"soldier", {11.5, 8.5}}}, kDoorway);
	Enemy& soldier = First();
	soldier.Alert();
	int standing = 0;
	for (double t = 0.0; t < 8.0; t += kTick) {
		scene_->Update(kTick);
		const bool held = soldier.InDoorway() && !soldier.IsMoving() &&
						  soldier.GetStateType() == EnemyStateType::Walk;
		standing = held ? standing + 1 : 0;
		EXPECT_LT(standing, 20) << t;
	}
	EXPECT_FALSE(soldier.InDoorway());
}

// Six soldiers round the player take turns: no more shoot at once than the
// difficulty lets, and every one of them gets its turns
TEST_F(TacticsTest, TheyTakeTurnsToShoot) {
	difficulty_ = {.enemy_damage = 0.0, .attackers = 2};  // the player lives
	Load({6.5, 8.5}, {{"soldier", {10.0, 8.5}},
					  {"soldier", {8.25, 11.53}},
					  {"soldier", {4.75, 11.53}},
					  {"soldier", {3.0, 8.5}},
					  {"soldier", {4.75, 5.47}},
					  {"soldier", {8.25, 5.47}}});
	const auto enemies = scene_->GetEnemies();
	std::array<int, 6> shots{};
	std::array<bool, 6> attacking{};
	int most = 0;
	for (double t = 0.0; t < 12.0; t += kTick) {
		scene_->Update(kTick);
		int now = 0;
		for (std::size_t i = 0; i < enemies.size(); ++i) {
			const bool at_it =
				enemies[i]->GetStateType() == EnemyStateType::Attack;
			shots[i] += at_it && !attacking[i] ? 1 : 0;
			attacking[i] = at_it;
			now += at_it ? 1 : 0;
		}
		most = std::max(most, now);
	}
	EXPECT_EQ(most, 2) << "at once";
	for (std::size_t i = 0; i < shots.size(); ++i) {
		EXPECT_GE(shots[i], 2) << "soldier " << i;
	}
}

}  // namespace
}  // namespace wolfenstein
