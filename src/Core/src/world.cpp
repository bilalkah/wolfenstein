#include "Core/world.h"
#include "Strike/weapon.h"
#include <iostream>
#include <utility>

namespace wolfenstein {

std::expected<std::unique_ptr<World>, std::string> World::Create(
	const TextureManager& textures, const std::string& asset_dir) {
	auto loader = SceneLoader::Open(asset_dir);
	if (!loader) {
		return std::unexpected(loader.error());
	}
	// The game is playable without sound, so a missing audio device is a
	// warning, not an error
	auto sound = SoundManager::Open(asset_dir + "sounds/");
	if (!sound) {
		std::cerr << "Sound disabled: " << sound.error() << '\n';
		sound = std::make_unique<SoundManager>();
	}
	return std::make_unique<World>(textures, std::move(*loader),
								   std::move(*sound));
}

World::World(const TextureManager& textures, SceneLoader loader,
			 std::unique_ptr<SoundManager> sound)
	: textures_(textures),
	  loader_(std::move(loader)),
	  sound_(std::move(sound)),
	  level_memory_(loader_.LargestLevelMemory()) {}

std::expected<void, std::string> World::NewGame(std::string_view weapon_name,
												std::string_view level,
												std::string_view difficulty) {
	const WeaponConfig* weapon = loader_.Config().FindWeapon(weapon_name);
	if (weapon == nullptr) {
		return std::unexpected("unknown weapon " + std::string(weapon_name));
	}
	const DifficultyConfig* chosen =
		loader_.Config().FindDifficulty(difficulty);
	if (chosen == nullptr) {
		return std::unexpected("unknown difficulty " + std::string(difficulty));
	}
	difficulty_ = chosen;
	// The old level borrows the old player: it goes first
	scene_.reset();
	const CharacterStats& stats = loader_.Config().player;
	// The level sets the start position
	CharacterConfig config(Position2D(), stats.translation_speed,
						   stats.rotation_speed, stats.width, stats.height);
	player_.emplace(config, *weapon, textures_, *sound_);
	level_index_ = 0;
	in_campaign_ = level.empty();
	return StartLevel(in_campaign_
						  ? std::string_view(loader_.Config().levels.front())
						  : level);
}

std::expected<void, std::string> World::NextLevel() {
	if (!HasNextLevel()) {
		return std::unexpected("no next level");
	}
	++level_index_;
	return StartLevel(loader_.Config().levels[level_index_]);
}

bool World::HasNextLevel() const {
	return in_campaign_ && level_index_ + 1 < loader_.Config().levels.size();
}

std::expected<void, std::string> World::StartLevel(
	std::string_view level_file) {
	if (!player_) {
		return std::unexpected("no game started");
	}
	const PreparedLevel* level = loader_.FindLevel(level_file);
	if (level == nullptr) {
		return std::unexpected("unknown level " + std::string(level_file));
	}
	// The previous level lives in the arena: it goes before the arena is
	// reused for the next one
	scene_.reset();
	level_memory_.Reset();
	scene_.emplace(textures_, *sound_, level->map, level->capacity,
				   level_memory_);
	level_ = level;
	scene_->SetDifficulty({.enemy_damage = difficulty_->enemy_damage,
						   .enemy_health = difficulty_->enemy_health,
						   .supplies = difficulty_->supplies});
	return loader_.Populate(*scene_, *level, *player_);
}

}  // namespace wolfenstein
