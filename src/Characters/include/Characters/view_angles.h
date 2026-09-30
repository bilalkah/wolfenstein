/**
 * @file view_angles.h
 * @brief Where the player looks, turned by the input frame by frame
 */

#ifndef CHARACTERS_INCLUDE_CHARACTERS_VIEW_ANGLES_H_
#define CHARACTERS_INCLUDE_CHARACTERS_VIEW_ANGLES_H_

#include "Characters/player.h"
#include "Characters/player_command.h"
#include "Math/vector.h"
#include <algorithm>

namespace karakale {

// Where the player looks, kept by the game rather than the simulation. The
// mouse and the turning keys move it every frame and the view is drawn from
// it at once; each tick's command carries it, and the player takes it.
// Turned only a tick at a time, the view was drawn between the last two
// ticks, a tick or two behind the hand, and a shot fired on the next tick
// went where the crosshair had not yet shown. A networked game keeps the
// view's angles on the player's own machine for the same reason.
class ViewAngles
{
  public:
	// Looking where the player does (a level starting, a game going on)
	void Reset(double theta, double pitch) {
		theta_ = theta;
		pitch_ = pitch;
	}
	// A frame's input, `seconds` long: its mouse look and turning move the
	// view, and the command carries the view instead
	PlayerCommand Apply(PlayerCommand command, double seconds) {
		theta_ = SumRadian(
			theta_,
			command.look + command.turn * Player::kKeyboardTurnSpeed * seconds);
		pitch_ = std::clamp(pitch_ + command.look_up, -Player::kMaxPitch,
							Player::kMaxPitch);
		command.look = 0.0;
		command.look_up = 0.0;
		command.turn = 0;
		command.has_view = true;
		command.view_theta = theta_;
		command.view_pitch = pitch_;
		return command;
	}
	double Theta() const { return theta_; }
	double Pitch() const { return pitch_; }

  private:
	double theta_ = 0.0;
	double pitch_ = 0.0;
};

}  // namespace karakale

#endif	// CHARACTERS_INCLUDE_CHARACTERS_VIEW_ANGLES_H_
