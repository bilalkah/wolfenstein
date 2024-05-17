#include "clock/clock.h"
#include <thread>

namespace wolfenstein {

void Clock::InitClock() {
	previos_time_point = std::chrono::high_resolution_clock::now();
}

void Clock::CalculateDeltaTime() {
	auto current_time_point = std::chrono::high_resolution_clock::now();
	delta_time = current_time_point - previos_time_point;
	previos_time_point = current_time_point;
}

double Clock::GetDeltaTime() {
	return delta_time.count();
}

double Clock::GetFramePerSecond() {
	return 1.0 / GetDeltaTime();
}

void Clock::SleepForHz(int hz) {
	if (hz == 0) {
		return;
	}
	std::chrono::duration<double, std::milli> duration_between_frame =
		std::chrono::high_resolution_clock::now() - previos_time_point;
	std::chrono::duration<double, std::milli> ms((1.0 / hz) * 1000);
	if (duration_between_frame.count() < ms.count()) {
		std::chrono::duration<double, std::milli> delta_ms(
			ms.count() - duration_between_frame.count());
		auto delta_ms_duration =
			std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
		std::this_thread::sleep_for(
			std::chrono::milliseconds(delta_ms_duration.count()));
	}
}

}  // namespace wolfenstein
