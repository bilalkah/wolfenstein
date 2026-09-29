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
	// Mouse look up (+) or down, as a share of the screen's height; applied
	// once
	double look_up = 0.0;
	bool fire = false;
	bool reload = false;
	// Opens the door in front
	bool use = false;
	// Takes the weapon with this index in hand (-1: none); applied once
	std::int8_t weapon = -1;
	// Steps to the next (+1) or previous (-1) weapon carried; applied once
	std::int8_t cycle = 0;
	// Where the player looks, set outright (ViewAngles: the game turns the
	// view as the mouse moves, frame by frame, and the tick takes it), with
	// look, look_up and turn already folded in
	bool has_view = false;
	double view_theta = 0.0;
	double view_pitch = 0.0;

	friend bool operator==(const PlayerCommand&,
						   const PlayerCommand&) = default;
};

// The input of a frame drawn after `earlier` frames and before the next
// tick, gathered with theirs for that tick: frames come faster than ticks
// (a 320 Hz screen draws five a tick), and a tick that took only the last
// frame's input lost the mouse's motion in the others, turning slower the
// faster the screen. Mouse motion adds up; a button pressed in any of the
// frames counts, so a click shorter than a tick still fires; movement is
// as it last was; a weapon chosen or stepped to waits for the tick.
inline PlayerCommand Gather(const PlayerCommand& earlier,
							const PlayerCommand& later) {
	PlayerCommand gathered = later;
	gathered.look += earlier.look;
	gathered.look_up += earlier.look_up;
	gathered.fire = earlier.fire || later.fire;
	gathered.reload = earlier.reload || later.reload;
	gathered.use = earlier.use || later.use;
	if (later.weapon < 0) {
		gathered.weapon = earlier.weapon;
	}
	if (later.cycle == 0) {
		gathered.cycle = earlier.cycle;
	}
	// The view as it last was
	if (!later.has_view && earlier.has_view) {
		gathered.has_view = true;
		gathered.view_theta = earlier.view_theta;
		gathered.view_pitch = earlier.view_pitch;
	}
	return gathered;
}

// The command again for a tick that has no new one of its own (a frame
// that runs two ticks, a server whose player's next command is late):
// moving, looking and holding the trigger as it was, but what is pressed
// once (use, reload, a weapon chosen or stepped to, mouse motion) not again
inline PlayerCommand Repeated(const PlayerCommand& command) {
	PlayerCommand repeated = command;
	repeated.look = 0.0;
	repeated.look_up = 0.0;
	repeated.use = false;
	repeated.reload = false;
	repeated.weapon = -1;
	repeated.cycle = 0;
	return repeated;
}

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_CHARACTERS_PLAYER_COMMAND_H_
