/**
 * @file saved_game.h
 * @brief A campaign in progress, kept between sessions
 */

#ifndef SETTINGS_INCLUDE_SETTINGS_SAVED_GAME_H
#define SETTINGS_INCLUDE_SETTINGS_SAVED_GAME_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace wolfenstein {

// The campaign as it stood when last saved (as a level starts, and when
// nothing is fighting the player): the level, what the player carries and
// where they stand, and what they have done there, so a later session can
// go on from there
struct SavedGame
{
	// Written into every save and required back: weapons are saved by their
	// index in the configuration, so a save from before the arsenal changed
	// (format 1 had a knife in the first slot) would give the wrong ones
	static constexpr unsigned kFormat = 2;
	// The most weapons a save keeps (as many as a player carries)
	static constexpr std::size_t kMaxWeapons = 8;
	// The most map cells whose exploration a save keeps (bits of `explored`)
	static constexpr std::size_t kMaxExploredCells = 2048;

	std::size_t level = 0;		 // in the campaign, from 0
	std::size_t weapon = 0;		 // the one in hand
	std::size_t difficulty = 1;	 // index into its difficulties
	// The game's roll for what its enemies carry: kept, so the game goes on
	// with the same drops (a save without one rolls as 0)
	std::uint64_t seed = 0;
	double health = 100.0;
	// The weapons carried, a bit per index into the configuration's; and
	// each weapon's rounds, in its magazine and besides it
	std::uint32_t weapons = 0;
	std::array<std::size_t, kMaxWeapons> ammo{};
	std::array<std::size_t, kMaxWeapons> reserve{};

	// Where the player stood; without it the level starts over
	bool has_position = false;
	double x = 0.0;
	double y = 0.0;
	double theta = 0.0;
	// The level's clock, and what is done in it: bit i of `killed` is the
	// level's enemy i, of `taken` its pickup i, of `explored` its map cell i
	// (row by row)
	double seconds = 0.0;
	std::uint64_t killed = 0;
	std::uint64_t taken = 0;
	std::uint32_t keys = 0;			 // held, as the game's key bits
	std::uint64_t secrets = 0;		 // bit i: the level's secret i was pushed
	std::size_t explored_cells = 0;	 // how many cells `explored` covers
	std::array<std::uint8_t, kMaxExploredCells / 8> explored{};

	// Read from "key=value" lines; nullopt if a field is missing or
	// malformed
	static std::optional<SavedGame> Parse(std::string_view text);
	// Writes those lines into `out`, NUL-terminated, and returns their
	// length (0 if `out` is too small); allocates nothing
	std::size_t Format(std::span<char> out) const;

	// The saved game, if there is one (read once, at startup)
	static std::optional<SavedGame> Load();
	// Replaces the saved game; allocates nothing, so it can happen in play
	void Save() const;
	static void Clear();

	friend bool operator==(const SavedGame&, const SavedGame&) = default;
};

}  // namespace wolfenstein

#endif	// SETTINGS_INCLUDE_SETTINGS_SAVED_GAME_H
