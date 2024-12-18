#include "TimeManager/time_manager.h"
#include <thread>

namespace wolfenstein {

TimeManager* TimeManager::instance_ = nullptr;

TimeManager& TimeManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new TimeManager();
	}
	return *instance_;
}

TimeManager::~TimeManager() {
	delete instance_;
}

void TimeManager::InitClock() {
	previos_time_point = std::chrono::high_resolution_clock::now();
	initial_time_point = previos_time_point;
}

void TimeManager::CalculateDeltaTime() {
	auto current_time_point = std::chrono::high_resolution_clock::now();
	delta_time = current_time_point - previos_time_point;
	previos_time_point = current_time_point;
}

void TimeManager::SleepForHz(double hz) {
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

double TimeManager::GetDeltaTime() {
	return delta_time.count();
}

double TimeManager::GetFramePerSecond() {
	return 1.0 / GetDeltaTime();
}

double TimeManager::GetCurrentTime() {
	auto time_passed =
		std::chrono::high_resolution_clock::now() - initial_time_point;
	auto current_time =
		std::chrono::duration_cast<std::chrono::seconds>(time_passed);
	return current_time.count();
}

std::string TimeManager::GetTime() {
	auto time_passed =
		std::chrono::high_resolution_clock::now() - initial_time_point;
	auto time_passed_seconds =
		std::chrono::duration_cast<std::chrono::seconds>(time_passed);
	auto time_passed_milliseconds =
		std::chrono::duration_cast<std::chrono::milliseconds>(time_passed);
	return std::to_string(time_passed_seconds.count()) + "." +
		   std::to_string(time_passed_milliseconds.count() % 1000) + "s";
}

}  // namespace wolfenstein
