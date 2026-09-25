#include "TimeManager/time_manager.h"
#include <thread>

namespace wolfenstein {

FrameClock::FrameClock() : previous_(Clock::now()) {}

void FrameClock::Restart() {
	previous_ = Clock::now();
}

void FrameClock::Tick() {
	const auto now = Clock::now();
	delta_ = fixed_delta_.count() > 0.0 ? fixed_delta_ : now - previous_;
	previous_ = now;
}

void FrameClock::SetFixedDeltaTime(double seconds) {
	fixed_delta_ = std::chrono::duration<double>(seconds);
}

void FrameClock::SleepForHz(double hz) const {
	if (hz <= 0.0) {
		return;
	}
	const std::chrono::duration<double> frame(1.0 / hz);
	const auto elapsed = Clock::now() - previous_;
	if (elapsed < frame) {
		std::this_thread::sleep_for(
			std::chrono::duration_cast<std::chrono::milliseconds>(frame -
																  elapsed));
	}
}

}  // namespace wolfenstein
