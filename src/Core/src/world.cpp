#include "Core/world.h"
#include "Strike/weapon.h"
#include <iostream>
#include <utility>

namespace wolfenstein {

namespace {
constexpr const char* kFirstLevel = "level1.json";
}  // namespace

std::expected<std::unique_ptr<World>, std::string> World::Create(
	const TextureManager& textures, std::string asset_dir) {
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
	  sound_(std::move(sound)) {}

std::expected<void, std::string> World::NewGame(
	const std::string& weapon_name) {
	const WeaponConfig* weapon = loader_.Config().FindWeapon(weapon_name);
	if (weapon == nullptr) {
		return std::unexpected("unknown weapon " + weapon_name);
	}
	// The old level borrows the old player: it goes first
	scene_.reset();
	const CharacterStats& stats = loader_.Config().player;
	// The level file sets the start position
	CharacterConfig config(Position2D(), stats.translation_speed,
						   stats.rotation_speed, stats.width, stats.height);
	player_ = std::make_unique<Player>(
		config, std::make_shared<Weapon>(*weapon, textures_, *sound_), *sound_);
	return LoadLevel(kFirstLevel);
}

std::expected<void, std::string> World::NextLevel() {
	return LoadLevel(scene_->GetNextScene());
}

bool World::HasNextLevel() const {
	return scene_ != nullptr && !scene_->GetNextScene().empty();
}

std::expected<void, std::string> World::LoadLevel(
	const std::string& level_file) {
	auto scene = loader_.Load(level_file, *player_, textures_, *sound_);
	if (!scene) {
		return std::unexpected(scene.error());
	}
	scene_ = std::move(*scene);
	return {};
}

}  // namespace wolfenstein
