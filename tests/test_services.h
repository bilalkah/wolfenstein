#ifndef TESTS_TEST_SERVICES_H
#define TESTS_TEST_SERVICES_H

#include "Core/level_data.h"
#include "SoundManager/sound_manager.h"
#include "TextureManager/texture_manager.h"
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace wolfenstein::testing {

// Textures for tests: no renderer, so no image is loaded, but every clip the
// enemies and weapons look up is defined, over placeholder ids
inline const TextureManager& TestTextures() {
	static TextureManager textures;
	static const bool defined = [] {
		constexpr std::uint16_t kFramesPerClip = 3;
		std::uint16_t next = 0;
		const auto define = [&](const std::string& owner, const char* clip) {
			textures.DefineCollection(owner + "_" + clip, next,
									  next + kFramesPerClip);
			next += kFramesPerClip;
		};
		for (const char* enemy : {"soldier", "caco_demon", "cyber_demon"}) {
			for (const char* clip :
				 {"idle", "walk", "attack", "pain", "death"}) {
				define(enemy, clip);
			}
		}
		for (const char* weapon : {"mp5", "shotgun"}) {
			for (const char* clip : {"loaded", "outofammo", "reload"}) {
				define(weapon, clip);
			}
		}
		// The levels' animated lights
		for (const char* light : {"green_light", "red_light"}) {
			textures.DefineCollection(light, next, next + kFramesPerClip);
			next += kFramesPerClip;
		}
		// The pickups' sprites
		for (const char* pickup : {"medkit", "large_medkit", "ammo_box"}) {
			textures.DefineTexture(pickup, next++);
		}
		textures.DefineTexture("door", next++);
		return true;
	}();
	(void)defined;
	return textures;
}

// The game's real content (assets/levels/config.json): tests play the
// shipped weapons and enemy types
inline const GameConfig& GameData() {
	static const GameConfig config = [] {
		std::ifstream file(std::string(RESOURCE_DIR) + "levels/config.json");
		auto parsed = ParseGameConfig(file);
		if (!parsed) {
			std::cerr << "config.json: " << parsed.error() << '\n';
			std::abort();
		}
		return std::move(*parsed);
	}();
	return config;
}

inline const WeaponConfig& Weapon(std::string_view name) {
	return *GameData().FindWeapon(name);
}

inline const EnemyConfig& Enemy(std::string_view type) {
	return GameData().enemies.find(type)->second;
}

// No audio device: effects are accepted and ignored
inline SoundManager& TestSound() {
	static SoundManager sound;
	return sound;
}

}  // namespace wolfenstein::testing

#endif	// TESTS_TEST_SERVICES_H
