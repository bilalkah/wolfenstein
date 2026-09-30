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

namespace karakale::testing {

// Defines on `textures` every clip the enemies and weapons look up, over
// placeholder ids: tests have no renderer, so load no image
inline void DefineTestTextures(TextureManager& textures) {
	constexpr std::uint16_t kFramesPerClip = 3;
	std::uint16_t next = 0;
	const auto define = [&](const std::string& owner, const char* clip) {
		textures.DefineCollection(owner + "_" + clip, next,
								  next + kFramesPerClip);
		next += kFramesPerClip;
	};
	for (const char* enemy : {"soldier", "caco_demon", "cyber_demon",
							  "shotgun_zombie", "minigun_zombie", "demon"}) {
		for (const char* clip : {"idle", "walk", "attack", "pain", "death"}) {
			define(enemy, clip);
		}
	}
	for (const char* weapon :
		 {"blade", "pistol", "mp5", "shotgun", "super_shotgun", "chainsaw",
		  "rocket_launcher", "plasma_rifle"}) {
		for (const char* clip : {"loaded", "outofammo", "reload"}) {
			define(weapon, clip);
		}
	}
	// A raise clip for the shotgun (a weapon may have one)
	define("shotgun", "raise");
	// Rockets and bolts in flight, and bursting
	for (const char* projectile : {"rocket", "plasma"}) {
		for (const char* clip : {"flight", "burst"}) {
			define(projectile, clip);
		}
	}
	// The levels' animated lights, and the puffs where shots land
	for (const char* light :
		 {"green_light", "red_light", "blood_puff", "dust_puff"}) {
		textures.DefineCollection(light, next, next + kFramesPerClip);
		next += kFramesPerClip;
	}
	// The pickups' sprites
	for (const char* pickup :
		 {"medkit", "large_medkit", "ammo_box", "clip", "mp5_pickup",
		  "shotgun_pickup", "super_shotgun_pickup", "chainsaw_pickup",
		  "rocket_launcher_pickup", "plasma_rifle_pickup", "intel"}) {
		textures.DefineTexture(pickup, next++);
	}
	for (const char* name : {"door", "door_gold", "door_silver", "gold_key",
							 "silver_key", "secret_mark", "bullet_mark"}) {
		textures.DefineTexture(name, next++);
	}
}

// The textures most tests share
inline const TextureManager& TestTextures() {
	static const TextureManager& textures = [] -> TextureManager& {
		static TextureManager defined;
		DefineTestTextures(defined);
		return defined;
	}();
	return textures;
}

// A silent melee weapon, made for the tests
inline const WeaponConfig& Blade() {
	static const WeaponConfig blade{.weapon_name = "blade",
									.label = "BLADE",
									.start = true,
									.attack_damage = {40.0, 40.0},
									.attack_range = 1.2,
									.attack_speed = 0.4,
									.reload_speed = 0.5};
	return blade;
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

}  // namespace karakale::testing

#endif	// TESTS_TEST_SERVICES_H
