// Checks every level of the campaign as it ships: whatever made the files
// (scripts/make_levels.py, or a hand edit), a level must be playable.

#include "Core/level_data.h"
#include "GameMap/map.h"
#include "test_services.h"
#include <cmath>
#include <deque>
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

// Every open cell reachable from `start`, moving between neighbours
std::set<Cell> Reachable(const Map& map, Cell start) {
	std::set<Cell> seen{start};
	std::deque<Cell> queue{start};
	while (!queue.empty()) {
		const auto [x, y] = queue.front();
		queue.pop_front();
		for (const Cell next :
			 {Cell{x + 1, y}, Cell{x - 1, y}, Cell{x, y + 1}, Cell{x, y - 1}}) {
			if (!map.IsWall(next.first, next.second) &&
				seen.insert(next).second) {
				queue.push_back(next);
			}
		}
	}
	return seen;
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

	const Cell start = CellOf(level->player.pose.x, level->player.pose.y);
	ASSERT_FALSE(map->IsBlocked(start.first, start.second)) << file;
	const auto reachable = Reachable(*map, start);

	EXPECT_FALSE(level->enemies.empty()) << file;
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
	EXPECT_TRUE(health) << file << " has no health to pick up";
	EXPECT_TRUE(ammo) << file << " has no ammunition to pick up";
}

INSTANTIATE_TEST_SUITE_P(Campaign, LevelDesign,
						 ::testing::ValuesIn(testing::GameData().levels),
						 [](const auto& info) {
							 std::string name = info.param;
							 return name.substr(0, name.find('.'));
						 });

}  // namespace
}  // namespace wolfenstein
