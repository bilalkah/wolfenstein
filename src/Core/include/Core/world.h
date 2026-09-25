/**
 * @file world.h
 * @brief The simulation side of the game and its single owner
 */

#ifndef CORE_INCLUDE_CORE_WORLD_H_
#define CORE_INCLUDE_CORE_WORLD_H_

#include "Characters/player.h"
#include "Core/scene.h"
#include "Core/scene_loader.h"
#include "SoundManager/sound_manager.h"
#include <expected>
#include <memory>
#include <string>

namespace wolfenstein {

class TextureManager;

// Owns everything the game simulates: the level loader and the game
// configuration, the sound, the player and the current level. The Game owns
// one World next to the presentation (window, renderers, menu), which
// borrows from it.
//
// Members are declared in dependency order, so they are destroyed in the
// reverse: the level (which borrows the player and the sound) first, then
// the player, then the sound. Nothing here is global, so tests and, later, a
// server can create worlds of their own.
class World
{
  public:
	// Reads the game configuration and opens the audio device (running
	// silently if there is none). The error says what could not be loaded.
	static std::expected<std::unique_ptr<World>, std::string> Create(
		const TextureManager& textures, const std::string& asset_dir);

	// The world borrows the textures, which outlive it
	World(const TextureManager& textures, SceneLoader loader,
		  std::unique_ptr<SoundManager> sound);
	// Levels and the player point back into it
	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;
	~World() = default;

	// A fresh player carrying the named weapon, in the first level. The
	// previous level and player are gone afterwards: views borrowing them
	// must be pointed at the new level before they draw again.
	std::expected<void, std::string> NewGame(const std::string& weapon_name);
	// Replaces the finished level with the next one (same caveat)
	std::expected<void, std::string> NextLevel();
	bool HasNextLevel() const;

	bool HasLevel() const { return scene_ != nullptr; }
	Scene& CurrentLevel() { return *scene_; }
	Player& GetPlayer() { return *player_; }
	SoundManager& Sound() { return *sound_; }
	const GameConfig& Config() const { return loader_.Config(); }

  private:
	std::expected<void, std::string> LoadLevel(const std::string& level_file);

	const TextureManager& textures_;
	SceneLoader loader_;
	std::unique_ptr<SoundManager> sound_;
	std::unique_ptr<Player> player_;
	std::unique_ptr<Scene> scene_;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_WORLD_H_
