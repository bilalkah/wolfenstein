#include "Animation/time_based_single_animation.h"

namespace wolfenstein {

TBSAnimation::TBSAnimation(const std::vector<uint16_t>& tex_ids,
						   const double animation_speed)
	: tex_ids(tex_ids),
	  current_frame(0),
	  animation_speed(animation_speed),
	  counter(0) {}

void TBSAnimation::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed) {
		current_frame = (current_frame + 1) % tex_ids.size();
		counter = 0;
	}
}

void TBSAnimation::Reset() {
	current_frame = 0;
	counter = 0;
}

int TBSAnimation::GetCurrentFrame() const {
	return tex_ids[current_frame];
}

}  // namespace wolfenstein
