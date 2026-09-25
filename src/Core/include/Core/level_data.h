/**
 * @file level_data.h
 * @brief Level and game configuration files parsed into plain structs
 */

#ifndef CORE_INCLUDE_CORE_LEVEL_DATA_H_
#define CORE_INCLUDE_CORE_LEVEL_DATA_H_

#include "Characters/character.h"
#include "Math/vector.h"
#include <expected>
#include <functional>
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace wolfenstein {

// The JSON tree exists only while a file is parsed: the game keeps these
// typed values, and a malformed file comes back as an error message rather
// than an exception from deep inside the loader (or, with operator[], a
// silently inserted null).

struct CharacterStats
{
	double translation_speed{};
	double rotation_speed{};
	double width{};
	double height{};
};

struct DynamicObjectStats
{
	double animation_speed{};
	double width{};
	double height{};
};

// config.json: the stats shared by every level
struct GameConfig
{
	// By enemy type ("soldier"); std::less<> allows string_view lookups
	std::map<std::string, CharacterStats, std::less<>> enemies;
	CharacterStats player;
	DynamicObjectStats light;
};

struct EnemySpawn
{
	std::string type;
	Position2D position;
};

struct ObjectSpawn
{
	std::string type;  // the animation clip, e.g. "green_light"
	vector2d position;
};

// levelN.json
struct LevelData
{
	std::string map;
	Position2D player;
	std::vector<EnemySpawn> enemies;
	std::vector<ObjectSpawn> dynamic_objects;
	std::string next_level;	 // empty for the last level
};

std::expected<GameConfig, std::string> ParseGameConfig(std::istream& input);
std::expected<LevelData, std::string> ParseLevel(std::istream& input);

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_LEVEL_DATA_H_
