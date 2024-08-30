#include "Animation/walk_animation.h"

namespace wolfenstein {

WalkAnimation::WalkAnimation(const std::vector<uint16_t>& tex_ids,
							 const double step_size)
	: tex_ids(tex_ids), current_frame(1), step_size(step_size), counter(0) {}

void WalkAnimation::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > step_size) {
		current_frame = (current_frame + 1) % tex_ids.size();
		counter = 0;
	}
}

void WalkAnimation::Reset() {
	current_frame = 1;
	counter = 0;
}

int WalkAnimation::GetCurrentFrame() const {
	return tex_ids[current_frame];
}

}  // namespace wolfenstein
