/**
 * @file saved_game.h
 * @brief A campaign in progress, kept between sessions
 */

#ifndef SETTINGS_INCLUDE_SETTINGS_SAVED_GAME_H
#define SETTINGS_INCLUDE_SETTINGS_SAVED_GAME_H

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace wolfenstein {

// Saved as each level of the campaign starts: which level, and what the
// player carried into it, so a later session can go on from there
struct SavedGame
{
	std::size_t level = 0;		 // in the campaign, from 0
	std::size_t weapon = 0;		 // index into the configuration's weapons
	std::size_t difficulty = 1;	 // index into its difficulties
	double health = 100.0;
	std::size_t ammo = 0;	  // in the magazine
	std::size_t reserve = 0;  // besides it

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
