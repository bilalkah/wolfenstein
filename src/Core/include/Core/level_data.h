/**
 * @file level_data.h
 * @brief Level and game configuration files parsed into plain structs
 */

#ifndef CORE_INCLUDE_CORE_LEVEL_DATA_H_
#define CORE_INCLUDE_CORE_LEVEL_DATA_H_

#include "Characters/character.h"
#include "Characters/enemy.h"
#include "GameObjects/pickup.h"
#include "Math/vector.h"
#include "Strike/weapon.h"
#include <expected>
#include <functional>
#include <istream>
#include <map>
#include <string>
#include <string_view>
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
	double radius{};  // the room its base takes on the floor
};

// A kind of pickup: how it looks and what it gives
struct PickupConfig
{
	std::string texture;  // a texture in textures.json
	double width{};
	double height{};
	PickupEffect effect;
};

// A difficulty the player can choose, and what it changes
struct DifficultyConfig
{
	std::string name;		  // "normal"
	std::string label;		  // shown when a new game starts
	std::string description;  // likewise
	double enemy_damage = 1.0;
	double enemy_health = 1.0;
	double supplies = 1.0;
};

// config.json: the game's content shared by every level. A new enemy type
// or weapon is added there, with its art in textures.json, not in code.
struct GameConfig
{
	// By enemy type ("soldier"); std::less<> allows string_view lookups
	std::map<std::string, EnemyConfig, std::less<>> enemies;
	// In the order the menu offers them
	std::vector<WeaponConfig> weapons;
	// By pickup type ("medkit")
	std::map<std::string, PickupConfig, std::less<>> pickups;
	// The campaign: level files in the order they are played
	std::vector<std::string> levels;
	// The level the benchmark plays; not part of the campaign
	std::string benchmark_level;
	// The track the menu plays (assets/music/<name>.mp3); optional
	std::string menu_music;
	// From easiest to hardest, as a new game offers them
	std::vector<DifficultyConfig> difficulties;
	CharacterStats player;
	DynamicObjectStats light;

	// The named weapon, or nullptr
	const WeaponConfig* FindWeapon(std::string_view name) const;
	// The named difficulty, or nullptr
	const DifficultyConfig* FindDifficulty(std::string_view name) const;
};

struct EnemySpawn
{
	std::string type;
	Position2D position;
	// One the level's kill_targets objective asks for
	bool target = false;
	// How far from its position it wanders while it knows of no player, to
	// spots in sight of it; 0 stands guard there
	double patrol_radius = 0.0;
};

// A secret: a wall that slides back along (dx, dy) when the player uses it,
// opening a hidden room
struct SecretSpawn
{
	int x = 0;
	int y = 0;
	int dx = 0;
	int dy = 0;
};

// What a level asks of the player before its exit opens; the text is shown
// on the HUD while it is not done
struct Objective
{
	enum class Type : std::uint8_t {
		KillAll,	  // every enemy in the level
		KillTargets,  // the enemies marked as targets
	};
	Type type = Type::KillAll;
	std::string text;
};

struct ObjectSpawn
{
	// The animation clip ("green_light"), or the pickup type ("medkit")
	std::string type;
	vector2d position;
};

// levelN.json
struct LevelData
{
	std::string name;  // shown when the level starts; optional
	// A few lines of story shown before the level starts; optional
	std::string briefing;
	// The track played through it (assets/music/<music>.mp3); optional,
	// silence without
	std::string music;
	std::string map;
	Position2D player;
	std::vector<EnemySpawn> enemies;
	std::vector<ObjectSpawn> dynamic_objects;
	// Optional in the file
	std::vector<ObjectSpawn> pickups;
	// Optional: a level with an exit (an X cell) and none only needs the
	// exit; a level without an exit ends when its enemies are all dead
	std::vector<Objective> objectives;
	// Optional
	std::vector<SecretSpawn> secrets;
};

std::expected<GameConfig, std::string> ParseGameConfig(std::istream& input);
std::expected<LevelData, std::string> ParseLevel(std::istream& input);

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_LEVEL_DATA_H_
