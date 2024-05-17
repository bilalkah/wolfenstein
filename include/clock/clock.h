/**
 * @file clock.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Clock class for delta time
 * @version 0.1
 * @date 2024-03-14
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef CLOCK_CLOCK_H_
#define CLOCK_CLOCK_H_

#include <chrono>

namespace wolfenstein {

class Clock
{
  public:
	Clock() = default;
    
	void InitClock();
	void CalculateDeltaTime();
	double GetDeltaTime();
	double GetFramePerSecond();
	void SleepForHz(int hz);

  private:
	std::chrono::duration<double> delta_time;
	std::chrono::time_point<std::chrono::high_resolution_clock>
		previos_time_point;
};

}  // namespace wolfenstein

#endif	// CLOCK_CLOCK_H_
