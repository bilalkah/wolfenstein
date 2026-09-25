#include "Animation/triggered_single_animation.h"
#include <algorithm>
#include <cmath>

namespace wolfenstein {

TriggeredSingleAnimation::TriggeredSingleAnimation(const uint16_t texture_id,
												   const double animation_speed,
												   int alpha_start,
												   int alpha_end)
	: texture_id(texture_id),
	  animation_speed(animation_speed),
	  alpha_start(alpha_start),
	  alpha_end(alpha_end) {}

void TriggeredSingleAnimation::Update(const double& delta_time) {
	progress_ = std::min(progress_ + delta_time * animation_speed, 1.0);
}

void TriggeredSingleAnimation::Reset() {
	progress_ = 0.0;
}

int TriggeredSingleAnimation::GetCurrentFrame() const {
	return texture_id;
}

bool TriggeredSingleAnimation::IsAnimationFinishedOnce() const {
	return progress_ >= 1.0;
}

std::uint8_t TriggeredSingleAnimation::GetAlpha() const {
	const double alpha = alpha_start + (alpha_end - alpha_start) * progress_;
	return static_cast<std::uint8_t>(std::clamp(std::lround(alpha), 0L, 255L));
}

}  // namespace wolfenstein
