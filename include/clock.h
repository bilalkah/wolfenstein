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

#ifndef CLOCK_H_
#define CLOCK_H_

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

namespace wolfenstein
{

class Clock
{
public:
  void
  InitClock()
  {
    previos_time_point = std::chrono::high_resolution_clock::now();
  }

  void
  CalculateDeltaTime()
  {
    auto current_time_point = std::chrono::high_resolution_clock::now();
    delta_time = current_time_point - previos_time_point;
    previos_time_point = current_time_point;
  }

  double
  GetDeltaTime()
  {
    return delta_time.count();
  }

  double
  GetFramePerSecond()
  {
    return 1.0 / GetDeltaTime();
  }

  void
  SleepForHz(int hz)
  {
    if(hz == 0)
      {
        return;
      }
    std::chrono::duration<double, std::milli> duration_between_frame
        = std::chrono::high_resolution_clock::now() - previos_time_point;
    std::chrono::duration<double, std::milli> ms((1.0 / hz) * 1000);
    if(duration_between_frame.count() < ms.count())
      {
        std::chrono::duration<double, std::milli> delta_ms(
            ms.count() - duration_between_frame.count());
        auto delta_ms_duration
            = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(delta_ms_duration.count()));
      }
  }

private:
  std::chrono::duration<double> delta_time;
  std::chrono::time_point<std::chrono::high_resolution_clock>
      previos_time_point;
};

} // namespace wolfenstein

#endif