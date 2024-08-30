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
#include <string>

namespace wolfenstein {

class TimeManager
{
  public:
	static TimeManager& GetInstance();

	TimeManager(const TimeManager&) = delete;
	TimeManager& operator=(const TimeManager&) = delete;
	~TimeManager() = default;

	void InitClock();
	void CalculateDeltaTime();
	void SleepForHz(double hz);

	double GetDeltaTime();
	double GetFramePerSecond();
	double GetCurrentTime();

	std::string GetTime();

  private:
	TimeManager() = default;
	static TimeManager* instance_;

	std::chrono::duration<double> delta_time;
	std::chrono::time_point<std::chrono::high_resolution_clock>
		previos_time_point;
	std::chrono::time_point<std::chrono::high_resolution_clock>
		initial_time_point;
};

}  // namespace wolfenstein

#endif	// TIME_MANAGER_INCLUDE_TIME_MANAGER_H_
