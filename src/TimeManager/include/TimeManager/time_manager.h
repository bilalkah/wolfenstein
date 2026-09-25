/**
 * @file time_manager.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef TIME_MANAGER_INCLUDE_TIME_MANAGER_H_
#define TIME_MANAGER_INCLUDE_TIME_MANAGER_H_

#include <chrono>

namespace wolfenstein {

// Measures the time between frames. The game owns it and hands each frame's
// delta to whatever advances with it, instead of everything reading a global
// clock.
class FrameClock
{
  public:
	FrameClock();

	// Times afresh from now, so time spent loading is not one long frame
	void Restart();
	// Ends a frame: the delta becomes the time since the previous Tick
	void Tick();
	// A positive value makes every frame advance by exactly this many
	// seconds, which makes runs reproducible (used by the benchmark)
	void SetFixedDeltaTime(double seconds);
	// Sleeps so that frames are at least 1 / hz seconds apart (0: no limit)
	void SleepForHz(double hz) const;
	// Seconds the last frame took
	double DeltaTime() const { return delta_.count(); }

  private:
	using Clock = std::chrono::steady_clock;

	Clock::time_point previous_;
	std::chrono::duration<double> delta_{};
	std::chrono::duration<double> fixed_delta_{};
};

}  // namespace wolfenstein

#endif	// TIME_MANAGER_INCLUDE_TIME_MANAGER_H_
