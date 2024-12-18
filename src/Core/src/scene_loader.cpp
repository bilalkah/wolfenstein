#include "Camera/single_raycaster.h"
#include "CollisionManager/collision_manager.h"
#include "Core/scene_loader.h"
#include "GameObjects/dynamic_object.h"
#include "NavigationManager/navigation_manager.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include "TimeManager/time_manager.h"

#include <fstream>
#include <iostream>
#include <string>

namespace wolfenstein {

SceneLoader* SceneLoader::instance_ = nullptr;

SceneLoader::SceneLoader() {
	asset_path = std::string(RESOURCE_DIR);
	std::ifstream file(asset_path + "levels/config.json");
	if (!file.is_open()) {
		std::cerr << "Unable to open config.json.\n";
		exit(EXIT_FAILURE);
	}

	file >> configs;
	file.close();
}

SceneLoader::~SceneLoader() {
	delete instance_;
}

SceneLoader& SceneLoader::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new SceneLoader();
	}
	return *instance_;
}

std::shared_ptr<Scene> SceneLoader::Load(std::string json_path,
										 std::shared_ptr<Player> player) {
	// Open the JSON file
	std::ifstream file(asset_path + "levels/" + json_path);
	if (!file.is_open()) {
		std::cerr << "Unable to open file.\n";
		exit(EXIT_FAILURE);
	}

	nlohmann::json level_data;
	file >> level_data;
	file.close();

	auto scene = std::make_shared<Scene>();

	scene->SetMap(std::make_shared<Map>(asset_path + "maps/" +
										level_data["map"].get<std::string>()));

	PreparePlayer(*scene, level_data["player"], player);
	PrepareEnemies(*scene, level_data["enemies"]);
	PrepareDynamicObjects(*scene, level_data["dynamicObjects"]);
	PrepareStaticObjects(*scene, level_data["staticObjects"]);

	scene->SetNextScene(level_data["next_level"].get<std::string>());
	InitManagers(scene);
	TimeManager::GetInstance().InitClock();
	return scene;
}

void SceneLoader::PreparePlayer(Scene& scene, nlohmann::json& player_data,
								std::shared_ptr<Player> player) {
	player->SetPosition(
		Position2D({player_data["position"]["x"].get<double>(),
					player_data["position"]["y"].get<double>()},
				   player_data["position"]["theta"].get<double>()));
	player->IncreaseHealth(100);
	scene.SetPlayer(player);
}

void SceneLoader::PrepareEnemies(Scene& scene, nlohmann::json& enemies) {
	for (const auto& enemy : enemies) {
		const auto enemy_config =
			configs["config_enemy"][enemy["type"].get<std::string>()];
		auto bot = EnemyFactory::CreateEnemy(
			enemy["type"].get<std::string>(),
			CharacterConfig(
				Position2D({enemy["position"]["x"].get<double>(),
							enemy["position"]["y"].get<double>()},
						   enemy["position"]["theta"].get<double>()),
				enemy_config["t_speed"].get<double>(),
				enemy_config["r_speed"].get<double>(),
				enemy_config["width"].get<double>(),
				enemy_config["height"].get<double>()));
		scene.AddObject(bot);
	}
}

void SceneLoader::PrepareDynamicObjects(Scene& scene,
										nlohmann::json& dynamic_objects) {
	const auto config_dynamic = configs["config_dynamic"]["light"];

	for (const auto& dynamic : dynamic_objects) {
		scene.AddObject(std::make_shared<DynamicObject>(
			vector2d(dynamic["position"]["x"].get<double>(),
					 dynamic["position"]["y"].get<double>()),
			std::make_unique<LoopedAnimation>(
				dynamic["type"].get<std::string>(),
				config_dynamic["animation_speed"].get<double>()),
			config_dynamic["width"].get<double>(),
			config_dynamic["height"].get<double>()));
	}
}

void SceneLoader::PrepareStaticObjects(Scene& scene,
									   nlohmann::json& static_objects) {}

void SceneLoader::InitManagers(const std::shared_ptr<Scene>& scene) {
	CollisionManager::GetInstance().InitManager(scene);
	NavigationManager::GetInstance().InitManager(scene);
	ShootingManager::GetInstance().InitManager(scene);
	SingleRayCasterService::GetInstance().InitService(scene);
	SoundManager::GetInstance().InitManager();
}

}  // namespace wolfenstein
