#include "Core/scene_loader.h"
#include "GameObjects/dynamic_object.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace karakale {

namespace {

// A number from 0 up to 1 for drop `entry` of the level's enemy `enemy`:
// the same for the same game (its seed) every time, and unrelated from one
// enemy, drop, level or game to the next. The level's map name is hashed
// (FNV-1a) in, and SplitMix64 mixes the lot.
double DropRoll(std::uint64_t seed, std::string_view level, std::size_t enemy,
				std::size_t entry) {
	std::uint64_t hash = 14695981039346656037ULL;
	for (const char c : level) {
		hash = (hash ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
	}
	std::uint64_t x = seed ^ hash ^ (enemy * 0x9E3779B97F4A7C15ULL) ^
					  ((entry + 1) * 0xBF58476D1CE4E5B9ULL);
	x += 0x9E3779B97F4A7C15ULL;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
	x ^= x >> 31;
	// The top 53 bits, as a double's fraction
	return static_cast<double>(x >> 11) * 0x1.0p-53;
}

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
	for (const std::string& file : loader.config_.arenas) {
		if (auto prepared = loader.Prepare(file); !prepared) {
			return std::unexpected(prepared.error());
		}
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
	// What the enemies may drop comes in as hidden pickups, after the
	// level's: one for everything each may drop, carried or not
	std::uint32_t drops = 0;
	for (const EnemySpawn& spawn : data->enemies) {
		const auto enemy = config_.enemies.find(spawn.type);
		if (enemy == config_.enemies.end()) {
			return std::unexpected(file + ": unknown enemy type " + spawn.type);
		}
		for (const EnemyDrop& drop : enemy->second.drops) {
			if (!config_.pickups.contains(drop.pickup)) {
				return std::unexpected(
					spawn.type + " drops an unknown pickup " + drop.pickup);
			}
			++drops;
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
		.pickups = static_cast<std::uint32_t>(data->pickups.size()) + drops,
		.secrets = static_cast<std::uint32_t>(data->secrets.size())};
	for (const SecretSpawn& secret : data->secrets) {
		const auto open = [&](int step) {
			return !map->IsBlocked(secret.x + step * secret.dx,
								   secret.y + step * secret.dy);
		};
		if (!map->IsWall(secret.x, secret.y) || !open(1) || !open(2)) {
			return std::unexpected(file + ": the secret at " +
								   std::to_string(secret.x) + "," +
								   std::to_string(secret.y) +
								   " is not a wall with room behind it");
		}
	}
	if (data->intel.size() > Scene::kIntel) {
		return std::unexpected(file + ": more intel than a level holds");
	}
	for (const IntelSpawn& page : data->intel) {
		if (!map->IsWall(page.x, page.y) ||
			map->IsBlocked(page.x + page.dx, page.y + page.dy)) {
			return std::unexpected(
				file + ": the intel at " + std::to_string(page.x) + "," +
				std::to_string(page.y) + " is not on a wall's open face");
		}
	}
	largest_memory_ =
		std::max(largest_memory_, Scene::MemoryFor(*map, capacity));
	largest_objects_ =
		std::max(largest_objects_,
				 data->enemies.size() + data->dynamic_objects.size() +
					 data->pickups.size() + drops + Scene::kSceneObjects);
	levels_.emplace(file, PreparedLevel{.data = std::move(*data),
										.map = std::move(*map),
										.capacity = capacity});
	return {};
}

std::vector<std::string> SceneLoader::MusicTracks() const {
	std::vector<std::string> tracks;
	const auto add = [&](const std::string& name) {
		if (!name.empty() && std::ranges::find(tracks, name) == tracks.end()) {
			tracks.push_back(name);
		}
	};
	add(config_.menu_music);
	for (const auto& [name, level] : levels_) {
		add(level.data.music);
	}
	return tracks;
}

const PreparedLevel* SceneLoader::FindLevel(std::string_view name) const {
	const auto found = levels_.find(name);
	return found == levels_.end() ? nullptr : &found->second;
}

std::expected<void, std::string> SceneLoader::Populate(
	Scene& scene, const PreparedLevel& level, std::uint64_t seed) const {
	const FigureStats& figure = config_.player_figure;
	scene.SetPlayerLook(figure.clips, figure.width, figure.height,
						figure.zones);
	for (const EnemySpawn& spawn : level.data.enemies) {
		// Checked when the level was prepared
		const EnemyConfig& enemy = config_.enemies.find(spawn.type)->second;
		if (!scene.AddEnemy(enemy, spawn.position)) {
			return std::unexpected("more enemies than the scene can hold");
		}
		scene.GetEnemies().back()->SetTarget(spawn.target);
		scene.GetEnemies().back()->SetPatrolRadius(spawn.patrol_radius);
	}
	const auto& light = config_.light;
	for (const ObjectSpawn& spawn : level.data.dynamic_objects) {
		const auto added =
			scene.AddDynamicObject(spawn.position,
								   LoopedAnimation(scene.Textures(), spawn.type,
												   light.animation_speed),
								   light.width, light.height, light.radius);
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
	// What each enemy may drop, after the level's own pickups (a saved game
	// counts them in this order), hidden: everything it may drop is made,
	// so the pickups are the same whatever it turns out to carry. Which it
	// does carry is the game's roll (its seed), the same each time the
	// level is loaded, and more likely the more supplies the difficulty
	// gives.
	const auto enemies = scene.GetEnemies();
	for (std::size_t i = 0; i < enemies.size(); ++i) {
		const auto& drops =
			config_.enemies.find(enemies[i]->GetBotName())->second.drops;
		for (std::size_t j = 0; j < drops.size(); ++j) {
			const PickupConfig& pickup =
				config_.pickups.find(drops[j].pickup)->second;
			if (!scene.AddPickup(enemies[i]->GetPose(),
								 scene.Textures().GetTextureId(pickup.texture),
								 pickup.width, pickup.height, pickup.effect)) {
				return std::unexpected("more pickups than the scene can hold");
			}
			Pickup* made = scene.GetPickups().back();
			made->MakeDrop();
			const double chance =
				std::min(drops[j].chance * scene.GetDifficulty().supplies, 1.0);
			if (DropRoll(seed, level.data.map, i, j) < chance) {
				enemies[i]->AddDrop(*made);
			}
		}
	}
	for (const SecretSpawn& secret : level.data.secrets) {
		if (!scene.GetMap().AddPushWall(secret.x, secret.y, secret.dx,
										secret.dy)) {
			return std::unexpected("a secret the map cannot hold");
		}
	}
	// Checked when the level was prepared
	for (const IntelSpawn& page : level.data.intel) {
		scene.AddIntel(page.x, page.y, page.dx, page.dy);
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

}  // namespace karakale
