#include "Core/scene_loader.h"
#include "GameObjects/dynamic_object.h"
#include "TimeManager/time_manager.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace wolfenstein {

SceneLoader* SceneLoader::instance_ = nullptr;

namespace {

// Opens and parses a file under the asset directory, exiting with the
// parser's message if it is missing or malformed: the game cannot run
// without its data
template <typename Parse>
auto LoadOrExit(const std::string& path, Parse parse) {
	std::ifstream file(path);
	if (!file.is_open()) {
		std::cerr << "Unable to open " << path << '\n';
		std::exit(EXIT_FAILURE);
	}
	auto parsed = parse(file);
	if (!parsed) {
		std::cerr << path << ": " << parsed.error() << '\n';
		std::exit(EXIT_FAILURE);
	}
	return std::move(*parsed);
}

}  // namespace

SceneLoader::SceneLoader()
	: asset_path(RESOURCE_DIR),
	  config_(LoadOrExit(asset_path + "levels/config.json", ParseGameConfig)) {}

SceneLoader::~SceneLoader() {
	delete instance_;
}

SceneLoader& SceneLoader::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new SceneLoader();
	}
	return *instance_;
}

std::unique_ptr<Scene> SceneLoader::Load(const std::string& json_path,
										 Player& player) {
	const LevelData level =
		LoadOrExit(asset_path + "levels/" + json_path, ParseLevel);

	// The level file says how many objects the scene must hold, so its pools
	// and arena are sized exactly, once
	auto scene = std::make_unique<Scene>(SceneCapacity{
		.enemies = static_cast<std::uint32_t>(level.enemies.size()),
		.dynamic_objects =
			static_cast<std::uint32_t>(level.dynamic_objects.size())});

	scene->SetMap(std::make_unique<Map>(asset_path + "maps/" + level.map));

	player.SetPosition(level.player);
	player.IncreaseHealth(100);
	scene->SetPlayer(player);
	PrepareEnemies(*scene, level);
	PrepareDynamicObjects(*scene, level);

	scene->SetNextScene(level.next_level);
	scene->FinishLoading();
	TimeManager::GetInstance().InitClock();
	return scene;
}

void SceneLoader::PrepareEnemies(Scene& scene, const LevelData& level) const {
	for (const auto& spawn : level.enemies) {
		const auto stats = config_.enemies.find(spawn.type);
		if (stats == config_.enemies.end()) {
			std::cerr << "Unknown enemy type in level: " << spawn.type << '\n';
			std::exit(EXIT_FAILURE);
		}
		const auto added = scene.AddEnemy(
			spawn.type,
			CharacterConfig(spawn.position, stats->second.translation_speed,
							stats->second.rotation_speed, stats->second.width,
							stats->second.height));
		if (!added) {
			std::cerr << "Level has more enemies than its scene can hold\n";
			std::exit(EXIT_FAILURE);
		}
	}
}

void SceneLoader::PrepareDynamicObjects(Scene& scene,
										const LevelData& level) const {
	const auto& light = config_.light;
	for (const auto& spawn : level.dynamic_objects) {
		const auto added = scene.AddDynamicObject(
			spawn.position, LoopedAnimation(spawn.type, light.animation_speed),
			light.width, light.height);
		if (!added) {
			std::cerr << "Level has more objects than its scene can hold\n";
			std::exit(EXIT_FAILURE);
		}
	}
}

}  // namespace wolfenstein
