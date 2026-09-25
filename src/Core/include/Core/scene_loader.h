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
#include <expected>
#include <memory>
#include <string>

namespace wolfenstein {

// Builds levels from the files under the asset directory. Owned by the
// World; it keeps the game configuration read when it was opened.
class SceneLoader
{
  public:
	// Reads the game configuration; the error says what is missing or wrong
	static std::expected<SceneLoader, std::string> Open(std::string asset_dir);

	// Builds the level described by levels/<level_file>; the caller owns it.
	// The scene borrows the player, the textures and the sound. The error
	// says what is wrong with the level's files.
	std::expected<std::unique_ptr<Scene>, std::string> Load(
		const std::string& level_file, Player& player,
		const TextureManager& textures, SoundManager& sound) const;

	const GameConfig& Config() const { return config_; }

  private:
	SceneLoader(std::string asset_dir, GameConfig config);
	std::expected<void, std::string> PrepareEnemies(
		Scene& scene, const LevelData& level) const;
	std::expected<void, std::string> PrepareDynamicObjects(
		Scene& scene, const LevelData& level) const;

	std::string asset_dir_;
	GameConfig config_;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_LOADER_H_
