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

#ifndef TIME_MANAGER_TIME_MANAGER_H_
#define TIME_MANAGER_TIME_MANAGER_H_

#include <chrono>

namespace wolfenstein {

class TimeManager
{
  public:
	TimeManager() = default;

	void InitClock();
	void CalculateDeltaTime();
	void SleepForHz(int hz);

	double GetDeltaTime();
	double GetFramePerSecond();
	double GetCurrentTime();

  private:
	std::chrono::duration<double> delta_time;
	std::chrono::time_point<std::chrono::high_resolution_clock>
		previos_time_point;
};

}  // namespace wolfenstein

#endif	// TIME_MANAGER_TIME_MANAGER_H_