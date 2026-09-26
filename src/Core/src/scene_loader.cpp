#include "Core/scene_loader.h"
#include "GameObjects/dynamic_object.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
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
	SceneLoader loader(std::move(asset_dir), std::move(*config));
	for (const std::string& file : loader.config_.levels) {
		if (auto prepared = loader.Prepare(file); !prepared) {
			return std::unexpected(prepared.error());
		}
	}
	if (auto prepared = loader.Prepare(loader.config_.benchmark_level);
		!prepared) {
		return std::unexpected(prepared.error());
	}
	return loader;
}

std::expected<void, std::string> SceneLoader::Prepare(const std::string& file) {
	if (levels_.contains(file)) {
		return {};
	}
	auto data = ParseFile(asset_dir_ + "levels/" + file, ParseLevel);
	if (!data) {
		return std::unexpected(data.error());
	}
	auto map = Map::FromFile(asset_dir_ + "maps/" + data->map);
	if (!map) {
		return std::unexpected(map.error());
	}
	for (const EnemySpawn& spawn : data->enemies) {
		if (!config_.enemies.contains(spawn.type)) {
			return std::unexpected(file + ": unknown enemy type " + spawn.type);
		}
	}
	for (const ObjectSpawn& spawn : data->pickups) {
		if (!config_.pickups.contains(spawn.type)) {
			return std::unexpected(file + ": unknown pickup type " +
								   spawn.type);
		}
	}
	const SceneCapacity capacity{
		.enemies = static_cast<std::uint32_t>(data->enemies.size()),
		.dynamic_objects =
			static_cast<std::uint32_t>(data->dynamic_objects.size()),
		.pickups = static_cast<std::uint32_t>(data->pickups.size())};
	largest_memory_ =
		std::max(largest_memory_, Scene::MemoryFor(*map, capacity));
	largest_objects_ = std::max(
		largest_objects_, data->enemies.size() + data->dynamic_objects.size() +
							  data->pickups.size());
	levels_.emplace(file, PreparedLevel{.data = std::move(*data),
										.map = std::move(*map),
										.capacity = capacity});
	return {};
}

const PreparedLevel* SceneLoader::FindLevel(std::string_view name) const {
	const auto found = levels_.find(name);
	return found == levels_.end() ? nullptr : &found->second;
}

std::expected<void, std::string> SceneLoader::Populate(
	Scene& scene, const PreparedLevel& level, Player& player) const {
	player.SetPosition(level.data.player);
	player.IncreaseHealth(100);
	player.SetKeys(0);	// the last level's keys open nothing here
	scene.SetPlayer(player);
	for (const EnemySpawn& spawn : level.data.enemies) {
		// Checked when the level was prepared
		const EnemyConfig& enemy = config_.enemies.find(spawn.type)->second;
		if (!scene.AddEnemy(enemy, spawn.position)) {
			return std::unexpected("more enemies than the scene can hold");
		}
		scene.GetEnemies().back()->SetTarget(spawn.target);
	}
	const auto& light = config_.light;
	for (const ObjectSpawn& spawn : level.data.dynamic_objects) {
		const auto added =
			scene.AddDynamicObject(spawn.position,
								   LoopedAnimation(scene.Textures(), spawn.type,
												   light.animation_speed),
								   light.width, light.height);
		if (!added) {
			return std::unexpected("more objects than the scene can hold");
		}
	}
	for (const ObjectSpawn& spawn : level.data.pickups) {
		// Checked when the level was prepared
		const PickupConfig& pickup = config_.pickups.find(spawn.type)->second;
		if (!scene.AddPickup(spawn.position,
							 scene.Textures().GetTextureId(pickup.texture),
							 pickup.width, pickup.height, pickup.effect)) {
			return std::unexpected("more pickups than the scene can hold");
		}
	}
	scene.SetGoals(std::ranges::any_of(level.data.objectives,
									   [](const Objective& objective) {
										   return objective.type ==
												  Objective::Type::KillAll;
									   }),
				   std::ranges::any_of(level.data.objectives,
									   [](const Objective& objective) {
										   return objective.type ==
												  Objective::Type::KillTargets;
									   }));
	scene.FinishLoading();
	return {};
}

}  // namespace wolfenstein
