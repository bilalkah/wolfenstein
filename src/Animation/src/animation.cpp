#include <Animation/animation.h>

namespace wolfenstein {

Animation::Animation(const std::vector<int>& tex_ids,
					 const double animation_speed)
	: tex_ids(tex_ids), current_frame(0), animation_speed(animation_speed) {}

void Animation::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed) {
		current_frame = (current_frame + 1) % tex_ids.size();
		counter = 0;
	}
}

void Animation::Reset() {
	current_frame = 0;
	counter = 0;
}

int Animation::GetCurrentFrame() const {
	return tex_ids[current_frame];
}

}  // namespace wolfenstein
