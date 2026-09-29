// Checks every level of the campaign as it ships: whatever made the files
// (scripts/make_levels.py, or a hand edit), a level must be playable.

#include "Core/level_data.h"
#include "GameMap/map.h"
#include "test_services.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <utility>

namespace wolfenstein {
namespace {

// How far from the start no enemy may stand, in map units
constexpr double kSafeRadius = 5.0;

using Cell = std::pair<int, int>;

Cell CellOf(double x, double y) {
	return {static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y))};
}

// Every open cell reachable from `start`, moving between neighbours, through
// doors the `keys` held open (unlocked doors always open) and through the
// `secrets`, which the player pushes aside
std::set<Cell> Reachable(const Map& map, Cell start, std::uint8_t keys = 0,
						 const std::set<Cell>& secrets = {}) {
	std::set<Cell> seen{start};
	std::deque<Cell> queue{start};
	while (!queue.empty()) {
		const auto [x, y] = queue.front();
		queue.pop_front();
		for (const Cell next :
			 {Cell{x + 1, y}, Cell{x - 1, y}, Cell{x, y + 1}, Cell{x, y - 1}}) {
			if (map.IsWall(next.first, next.second) &&
				!secrets.contains(next)) {
				continue;
			}
			const Door* door = map.FindDoor(next.first, next.second);
			if (door != nullptr &&
				(keys & KeyBit(door->lock)) != KeyBit(door->lock)) {
				continue;  // locked, and its key not yet found
			}
			if (seen.insert(next).second) {
				queue.push_back(next);
			}
		}
	}
	return seen;
}

// What the player can reach in the end: from the start, picking up every key
// reachable so far and going through the doors it opens, until nothing new
// opens. A key only reachable through its own door is never found.
std::set<Cell> ReachableWithKeys(const Map& map, Cell start,
								 const LevelData& level) {
	std::set<Cell> secrets;
	for (const SecretSpawn& secret : level.secrets) {
		secrets.insert({secret.x, secret.y});
	}
	std::uint8_t keys = 0;
	for (;;) {
		const auto reachable = Reachable(map, start, keys, secrets);
		std::uint8_t found = keys;
		for (const ObjectSpawn& pickup : level.pickups) {
			const auto type = testing::GameData().pickups.find(pickup.type);
			if (type != testing::GameData().pickups.end() &&
				reachable.contains(
					CellOf(pickup.position.x, pickup.position.y))) {
				found |= type->second.effect.keys;
			}
		}
		if (found == keys) {
			return reachable;
		}
		keys = found;
	}
}

class LevelDesign : public ::testing::TestWithParam<std::string>
{};

TEST_P(LevelDesign, IsPlayable) {
	const std::string& file = GetParam();
	std::ifstream input(std::string(RESOURCE_DIR) + "levels/" + file);
	const auto level = ParseLevel(input);
	ASSERT_TRUE(level) << level.error();
	const auto map =
		Map::FromFile(std::string(RESOURCE_DIR) + "maps/" + level->map);
	ASSERT_TRUE(map) << map.error();

	// A solid border: nothing walks or sees off the map
	const int width = map->GetSizeX();
	const int height = map->GetSizeY();
	for (int x = 0; x < width; ++x) {
		EXPECT_TRUE(map->IsBlocked(x, 0) && map->IsBlocked(x, height - 1))
			<< file << " row " << x;
	}
	for (int y = 0; y < height; ++y) {
		EXPECT_TRUE(map->IsBlocked(0, y) && map->IsBlocked(width - 1, y))
			<< file << " column " << y;
	}

	// A save keeps a bit for each of its pickups, its own and all its
	// enemies may drop, and for each enemy: 64 of each at most
	std::size_t pickups = level->pickups.size();
	for (const EnemySpawn& enemy : level->enemies) {
		pickups += testing::Enemy(enemy.type).drops.size();
	}
	EXPECT_LE(pickups, 64u) << file;
	EXPECT_LE(level->enemies.size(), 64u) << file;

	const Cell start = CellOf(level->player.pose.x, level->player.pose.y);
	ASSERT_FALSE(map->IsBlocked(start.first, start.second)) << file;
	const auto reachable = ReachableWithKeys(*map, start, *level);

	// Every lock has its key in the level
	std::uint8_t keys_in_level = 0;
	for (const ObjectSpawn& pickup : level->pickups) {
		const auto type = testing::GameData().pickups.find(pickup.type);
		if (type != testing::GameData().pickups.end()) {
			keys_in_level |= type->second.effect.keys;
		}
	}
	for (const Door& door : map->GetDoors()) {
		EXPECT_EQ(keys_in_level & KeyBit(door.lock), KeyBit(door.lock))
			<< file << ": the door at " << door.x << "," << door.y
			<< " has no key in the level";
		EXPECT_TRUE(reachable.contains({door.x, door.y}))
			<< file << ": the door at " << door.x << "," << door.y
			<< " cannot be reached";
	}

	EXPECT_FALSE(level->enemies.empty()) << file;
	// Its track is there to play
	EXPECT_FALSE(level->music.empty()) << file << " plays no music";
	EXPECT_TRUE(std::filesystem::exists(std::string(RESOURCE_DIR) + "music/" +
										level->music + ".mp3"))
		<< file << ": no track " << level->music;

	for (const EnemySpawn& enemy : level->enemies) {
		const Cell cell = CellOf(enemy.position.pose.x, enemy.position.pose.y);
		EXPECT_TRUE(reachable.contains(cell))
			<< file << ": a " << enemy.type << " at " << cell.first << ","
			<< cell.second << " is in a wall or cut off";
		EXPECT_GE(std::hypot(enemy.position.pose.x - level->player.pose.x,
							 enemy.position.pose.y - level->player.pose.y),
				  kSafeRadius)
			<< file << ": a " << enemy.type << " stands at the start";
		EXPECT_TRUE(testing::GameData().enemies.contains(enemy.type)) << file;
		// A patroller walks about near its post, not across the level
		EXPECT_LE(enemy.patrol_radius, 6.0)
			<< file << ": a " << enemy.type << " wanders too far";
	}
	for (const ObjectSpawn& object : level->dynamic_objects) {
		EXPECT_GE(std::hypot(object.position.x - level->player.pose.x,
							 object.position.y - level->player.pose.y),
				  1.0)
			<< file << ": a light stands at the start";
		const Cell cell = CellOf(object.position.x, object.position.y);
		EXPECT_TRUE(reachable.contains(cell))
			<< file << ": a light at " << cell.first << "," << cell.second
			<< " is in a wall or cut off";
	}

	// Supplies: health and ammunition somewhere in every level, each where
	// the player can walk to it
	bool health = false;
	bool ammo = false;
	for (const ObjectSpawn& pickup : level->pickups) {
		const auto type = testing::GameData().pickups.find(pickup.type);
		ASSERT_NE(type, testing::GameData().pickups.end())
			<< file << ": unknown pickup " << pickup.type;
		health = health || type->second.effect.health > 0.0;
		ammo = ammo || type->second.effect.ammo_boxes > 0;
		const Cell cell = CellOf(pickup.position.x, pickup.position.y);
		EXPECT_TRUE(reachable.contains(cell))
			<< file << ": a " << pickup.type << " at " << cell.first << ","
			<< cell.second << " is in a wall or cut off";
		EXPECT_GE(std::hypot(pickup.position.x - level->player.pose.x,
							 pickup.position.y - level->player.pose.y),
				  1.0)
			<< file << ": a " << pickup.type << " lies at the start";
	}
	// Secrets: a wall with room behind it for the wall to slide into, reached
	// from the level
	for (const SecretSpawn& secret : level->secrets) {
		EXPECT_TRUE(map->IsWall(secret.x, secret.y))
			<< file << ": the secret at " << secret.x << "," << secret.y
			<< " is not a wall";
		for (int step = 1; step <= PushWall::kDistance; ++step) {
			EXPECT_FALSE(map->IsWall(secret.x + step * secret.dx,
									 secret.y + step * secret.dy))
				<< file << ": the secret at " << secret.x << "," << secret.y
				<< " has no room to slide";
		}
		EXPECT_TRUE(reachable.contains({secret.x, secret.y}))
			<< file << ": the secret at " << secret.x << "," << secret.y
			<< " cannot be reached";
	}

	// A way out, and what it waits for
	ASSERT_TRUE(map->HasExit()) << file << " has no exit";
	const vector2i exit = map->GetExit();
	const bool exit_reachable = std::ranges::any_of(
		std::array{Cell{exit.x + 1, exit.y}, Cell{exit.x - 1, exit.y},
				   Cell{exit.x, exit.y + 1}, Cell{exit.x, exit.y - 1}},
		[&](const Cell& cell) { return reachable.contains(cell); });
	EXPECT_TRUE(exit_reachable) << file << ": the exit cannot be reached";
	EXPECT_FALSE(level->objectives.empty()) << file << " has no objectives";
	EXPECT_FALSE(level->briefing.empty()) << file << " has no briefing";
	for (const Objective& objective : level->objectives) {
		EXPECT_FALSE(objective.text.empty()) << file;
		if (objective.type == Objective::Type::KillTargets) {
			EXPECT_TRUE(
				std::ranges::any_of(level->enemies, &EnemySpawn::target))
				<< file << ": kill_targets without a target";
		}
	}

	EXPECT_TRUE(health) << file << " has no health to pick up";
	EXPECT_TRUE(ammo) << file << " has no ammunition to pick up";
}

// The menu's track is there to play
TEST(Music, TheMenuHasItsTrack) {
	const std::string& menu = testing::GameData().menu_music;
	ASSERT_FALSE(menu.empty());
	EXPECT_TRUE(std::filesystem::exists(std::string(RESOURCE_DIR) + "music/" +
										menu + ".mp3"));
}

INSTANTIATE_TEST_SUITE_P(Campaign, LevelDesign,
						 ::testing::ValuesIn(testing::GameData().levels),
						 [](const auto& info) {
							 std::string name = info.param;
							 return name.substr(0, name.find('.'));
						 });

}  // namespace
}  // namespace wolfenstein
