/**
 * @file player_command.h
 * @brief One tick of player input, independent of any input device
 */

#ifndef CHARACTERS_INCLUDE_CHARACTERS_PLAYER_COMMAND_H_
#define CHARACTERS_INCLUDE_CHARACTERS_PLAYER_COMMAND_H_

#include <cstdint>

namespace wolfenstein {

// What the player wants to do this tick. The game samples it from the
// keyboard and mouse; the simulation only ever sees commands, so it can be
// driven by a test, a replay or (for multiplayer) a network peer, and gives
// the same result for the same commands.
struct PlayerCommand
{
	// Each -1, 0 or 1: +1 is forward, right, and clockwise respectively
	std::int8_t forward = 0;
	std::int8_t strafe = 0;
	std::int8_t turn = 0;  // keyboard turning, at a fixed rate
	// Mouse look, in radians; applied once, not per tick
	double look = 0.0;
	bool fire = false;
	bool reload = false;
	// Opens the door in front
	bool use = false;
	// Takes the weapon with this index in hand (-1: none); applied once
	std::int8_t weapon = -1;
	// Steps to the next (+1) or previous (-1) weapon carried; applied once
	std::int8_t cycle = 0;

	friend bool operator==(const PlayerCommand&,
						   const PlayerCommand&) = default;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_CHARACTERS_PLAYER_COMMAND_H_
