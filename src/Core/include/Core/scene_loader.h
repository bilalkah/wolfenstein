/**
 * @file scene.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-13
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CORE_INCLUDE_CORE_SCENE_LOADER_H_
#define CORE_INCLUDE_CORE_SCENE_LOADER_H_

#include "Core/level_data.h"
#include "Core/scene.h"
#include "GameMap/map.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace wolfenstein {

// A level ready to play: its file parsed and its map read at startup
struct PreparedLevel
{
	LevelData data;
	Map map;
	SceneCapacity capacity;
};

// The game's content, read once at startup: the configuration and every
// level it names (the campaign and the benchmark level). Starting a level
// later only reads what is here, so it needs no file access or allocation.
// Owned by the World.
class SceneLoader
{
  public:
	// Reads the configuration and every level; the error says which file is
	// missing or wrong
	static std::expected<SceneLoader, std::string> Open(std::string asset_dir);

	const GameConfig& Config() const { return config_; }
	// The level with this file name, or nullptr
	const PreparedLevel* FindLevel(std::string_view name) const;
	// Every track the game plays, the menu's and the levels', once each
	std::vector<std::string> MusicTracks() const;
	// The arena bytes and object count of the largest level: what the level
	// arena and the per-object views are sized for, once
	std::size_t LargestLevelMemory() const { return largest_memory_; }
	std::size_t LargestLevelObjects() const { return largest_objects_; }

	// Fills a scene built for `level` with its enemies and objects, places the
	// player in it and builds its navigation. What each enemy carries to
	// drop is rolled from `seed` (a game's): the same seed, the same drops.
	// The error says what the level asks for that the configuration lacks.
	std::expected<void, std::string> Populate(Scene& scene,
											  const PreparedLevel& level,
											  Player& player,
											  std::uint64_t seed = 0) const;

  private:
	SceneLoader(std::string asset_dir, GameConfig config);
	std::expected<void, std::string> Prepare(const std::string& file);

	std::string asset_dir_;
	GameConfig config_;
	std::map<std::string, PreparedLevel, std::less<>> levels_;
	std::size_t largest_memory_ = 0;
	std::size_t largest_objects_ = 0;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_LOADER_H_
