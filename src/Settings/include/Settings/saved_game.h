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
	// The most map cells whose exploration a save keeps (bits of `explored`)
	static constexpr std::size_t kMaxExploredCells = 2048;

	std::size_t level = 0;		 // in the campaign, from 0
	std::size_t weapon = 0;		 // index into the configuration's weapons
	std::size_t difficulty = 1;	 // index into its difficulties
	double health = 100.0;
	std::size_t ammo = 0;	  // in the magazine
	std::size_t reserve = 0;  // besides it

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
