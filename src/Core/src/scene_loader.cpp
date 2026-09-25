#include "Core/scene_loader.h"
#include "GameObjects/dynamic_object.h"
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

namespace wolfenstein {

namespace {

// Opens and parses a file, naming the file in any error
template <typename Parse>
auto ParseFile(const std::string& path, Parse parse)
	-> decltype(parse(std::declval<std::istream&>())) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return std::unexpected("cannot open " + path);
	}
	auto parsed = parse(file);
	if (!parsed) {
		return std::unexpected(path + ": " + parsed.error());
	}
	return parsed;
}

}  // namespace

SceneLoader::SceneLoader(std::string asset_dir, GameConfig config)
	: asset_dir_(std::move(asset_dir)), config_(std::move(config)) {}

std::expected<SceneLoader, std::string> SceneLoader::Open(
	std::string asset_dir) {
	auto config = ParseFile(asset_dir + "levels/config.json", ParseGameConfig);
	if (!config) {
		return std::unexpected(config.error());
	}
	return SceneLoader(std::move(asset_dir), std::move(*config));
}

std::expected<std::unique_ptr<Scene>, std::string> SceneLoader::Load(
	const std::string& level_file, Player& player,
	const TextureManager& textures, SoundManager& sound) const {
	const auto level =
		ParseFile(asset_dir_ + "levels/" + level_file, ParseLevel);
	if (!level) {
		return std::unexpected(level.error());
	}
	auto map = Map::FromFile(asset_dir_ + "maps/" + level->map);
	if (!map) {
		return std::unexpected(map.error());
	}

	// The level file says how many objects the scene must hold, so its pools
	// and arena are sized exactly, once
	auto scene = std::make_unique<Scene>(
		textures, sound, *map,
		SceneCapacity{
			.enemies = static_cast<std::uint32_t>(level->enemies.size()),
			.dynamic_objects =
				static_cast<std::uint32_t>(level->dynamic_objects.size())});

	player.SetPosition(level->player);
	player.IncreaseHealth(100);
	scene->SetPlayer(player);
	if (auto added = PrepareEnemies(*scene, *level); !added) {
		return std::unexpected(level_file + ": " + added.error());
	}
	if (auto added = PrepareDynamicObjects(*scene, *level); !added) {
		return std::unexpected(level_file + ": " + added.error());
	}

	scene->SetNextScene(level->next_level);
	scene->FinishLoading();
	return scene;
}

std::expected<void, std::string> SceneLoader::PrepareEnemies(
	Scene& scene, const LevelData& level) const {
	for (const auto& spawn : level.enemies) {
		const auto enemy = config_.enemies.find(spawn.type);
		if (enemy == config_.enemies.end()) {
			return std::unexpected("unknown enemy type " + spawn.type);
		}
		const auto added = scene.AddEnemy(enemy->second, spawn.position);
		if (!added) {
			return std::unexpected("more enemies than the scene can hold");
		}
	}
	return {};
}

std::expected<void, std::string> SceneLoader::PrepareDynamicObjects(
	Scene& scene, const LevelData& level) const {
	const auto& light = config_.light;
	for (const auto& spawn : level.dynamic_objects) {
		const auto added =
			scene.AddDynamicObject(spawn.position,
								   LoopedAnimation(scene.Textures(), spawn.type,
												   light.animation_speed),
								   light.width, light.height);
		if (!added) {
			return std::unexpected("more objects than the scene can hold");
		}
	}
	return {};
}

}  // namespace wolfenstein
