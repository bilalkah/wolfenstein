/**
 * @file world.h
 * @brief The simulation side of the game and its single owner
 */

#ifndef CORE_INCLUDE_CORE_WORLD_H_
#define CORE_INCLUDE_CORE_WORLD_H_

#include "Characters/player.h"
#include "Core/scene.h"
#include "Core/scene_loader.h"
#include "Settings/saved_game.h"
#include "SoundManager/sound_manager.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace wolfenstein {

class TextureManager;

// Owns everything the game simulates: the game content (configuration and
// levels, read at startup), the sound, the player and the current level. The
// Game owns one World next to the presentation (window, renderers, menu),
// which borrows from it.
//
// Everything it needs while the game runs is set up when it is created: one
// arena sized for the largest level, reset and reused for each level, and
// room for the player and the scene, built in place. Starting a game or a
// level allocates nothing.
//
// Members are declared in dependency order, so they are destroyed in the
// reverse: the level (which borrows the player, the sound and the arena)
// first. Nothing here is global, so tests and, later, a server can create
// worlds of their own.
class World
{
  public:
	// Reads the game content and opens the audio device (running silently if
	// there is none). The error says what could not be loaded.
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

	// A fresh player carrying the named weapon, in the campaign's first level
	// (or in `level`, e.g. the benchmark's), at the named difficulty. The
	// previous level and player are gone afterwards: views borrowing them
	// must be pointed at the new level before they draw again.
	// `seed` rolls what its enemies carry to drop: the same seed, the same
	// drops.
	std::expected<void, std::string> NewGame(
		std::string_view weapon_name, std::string_view level = {},
		std::string_view difficulty = "normal", std::uint64_t seed = 0);
	// Goes on with a saved game: its level, with the player where it stood
	// and carrying what it carried, and the level as it was left (enemies
	// killed, pickups taken, the map explored, the clock)
	std::expected<void, std::string> ContinueGame(const SavedGame& saved);
	// The campaign as it stands, to save; nullopt outside the campaign.
	// Allocates nothing: games are saved while they run.
	std::optional<SavedGame> Capture() const;
	// Whether it is a moment to save: nothing is fighting the player, and
	// nothing has hurt it for a little while
	bool IsQuiet() const;
	// Replaces the finished level with the campaign's next one (same caveat)
	std::expected<void, std::string> NextLevel();
	bool HasNextLevel() const;
	// The campaign's level `index` (from 0), or nullptr
	const PreparedLevel* FindCampaignLevel(std::size_t index) const {
		const auto& levels = loader_.Config().levels;
		return index < levels.size() ? loader_.FindLevel(levels[index])
									 : nullptr;
	}
	// Playing the campaign, not a level on its own (the benchmark's)
	bool InCampaign() const { return in_campaign_; }
	// The difficulty of the game being played
	const DifficultyConfig& Difficulty() const { return *difficulty_; }
	// 1 for the campaign's first level
	std::size_t LevelNumber() const { return level_index_ + 1; }
	// What the current level asks of the player, from its file
	std::span<const Objective> LevelObjectives() const {
		return level_->data.objectives;
	}
	// The current level's briefing from its file; empty if it has none
	std::string_view LevelBriefing() const { return level_->data.briefing; }
	std::string_view LevelDebrief() const { return level_->data.debrief; }
	// The level's pages of intel, as its file writes them
	std::span<const IntelSpawn> LevelIntel() const {
		return level_->data.intel;
	}
	// The level's place in the campaign, from 0
	std::size_t LevelIndex() const { return level_index_; }
	// The current level's name from its file; empty if it has none
	std::string_view LevelName() const { return level_->data.name; }

	bool HasLevel() const { return scene_.has_value(); }
	// Both exist once NewGame succeeded
	Scene& CurrentLevel() {
		assert(scene_ && "no level loaded");
		return *scene_;
	}
	Player& GetPlayer() {
		assert(player_ && "no game started");
		return *player_;
	}
	SoundManager& Sound() { return *sound_; }
	const GameConfig& Config() const { return loader_.Config(); }
	// The most objects any level has: what per-object views are sized for
	std::size_t LargestLevelObjects() const {
		return loader_.LargestLevelObjects();
	}

  private:
	std::expected<void, std::string> StartLevel(std::string_view level_file);

	const TextureManager& textures_;
	SceneLoader loader_;
	std::unique_ptr<SoundManager> sound_;
	memory::MonotonicArena level_memory_;
	std::optional<Player> player_;
	std::optional<Scene> scene_;
	const DifficultyConfig* difficulty_ = nullptr;
	const PreparedLevel* level_ = nullptr;
	std::size_t level_index_ = 0;
	bool in_campaign_ = true;
	std::uint64_t seed_ = 0;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_WORLD_H_
