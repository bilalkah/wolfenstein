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

#include "Core/json.hpp"
#include "Core/scene.h"

namespace wolfenstein {

class SceneLoader
{
  public:
	~SceneLoader();
	SceneLoader(const SceneLoader&) = delete;
	SceneLoader& operator=(const SceneLoader&) = delete;
	static SceneLoader& GetInstance();

	std::shared_ptr<Scene> Load(std::string json_path,
								std::shared_ptr<Player> player);

  private:
	SceneLoader();
	void PreparePlayer(Scene& scene, nlohmann::json& player_data,
					   std::shared_ptr<Player> player);
	void PrepareEnemies(Scene& scene, nlohmann::json& enemies);
	void PrepareDynamicObjects(Scene& scene, nlohmann::json& dynamic_objects);
	void PrepareStaticObjects(Scene& scene, nlohmann::json& static_objects);
	void InitManagers(const std::shared_ptr<Scene>& scene);
	static SceneLoader* instance_;
	std::string asset_path;
	nlohmann::json configs;
};	// class SceneLoader

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_LOADER_H_
