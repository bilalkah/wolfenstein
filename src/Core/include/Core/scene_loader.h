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
#include <memory>
#include <string>

namespace wolfenstein {

class SceneLoader
{
  public:
	~SceneLoader();
	SceneLoader(const SceneLoader&) = delete;
	SceneLoader& operator=(const SceneLoader&) = delete;
	static SceneLoader& GetInstance();

	// Builds the level; the caller owns it. The scene borrows the player.
	std::unique_ptr<Scene> Load(const std::string& json_path, Player& player);

  private:
	SceneLoader();
	void PrepareEnemies(Scene& scene, const LevelData& level) const;
	void PrepareDynamicObjects(Scene& scene, const LevelData& level) const;
	static SceneLoader* instance_;
	std::string asset_path;
	GameConfig config_;
};	// class SceneLoader

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_LOADER_H_
