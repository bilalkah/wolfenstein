#include "Animation/triggered_single_animation.h"
#include <algorithm>
#include <cmath>

namespace wolfenstein {

TriggeredSingleAnimation::TriggeredSingleAnimation(double animation_speed,
												   int alpha_start,
												   int alpha_end)
	: animation_speed(animation_speed),
	  alpha_start(alpha_start),
	  alpha_end(alpha_end) {}

void TriggeredSingleAnimation::Update(double delta_time) {
	progress_ = std::min(progress_ + delta_time * animation_speed, 1.0);
}

void TriggeredSingleAnimation::Reset() {
	progress_ = 0.0;
}

bool TriggeredSingleAnimation::IsAnimationFinishedOnce() const {
	return progress_ >= 1.0;
}

std::uint8_t TriggeredSingleAnimation::GetAlpha() const {
	const double alpha = alpha_start + (alpha_end - alpha_start) * progress_;
	return static_cast<std::uint8_t>(std::clamp(std::lround(alpha), 0L, 255L));
}

}  // namespace wolfenstein
