#include "Animation/looped_animation.h"
#include "TextureManager/texture_manager.h"
namespace wolfenstein {

LoopedAnimation::LoopedAnimation(const std::vector<uint16_t>& textures,
								 const double animation_speed)
	: textures(textures),
	  textures_size(textures.size()),
	  current_frame(0),
	  animation_speed(animation_speed),
	  counter(0),
	  is_animation_finished_once(false) {}

LoopedAnimation::LoopedAnimation(const std::string collection_name,
								 const double animation_speed)
	: textures(
		  TextureManager::GetInstance().GetTextureCollection(collection_name)),
	  textures_size(textures.size()),
	  current_frame(0),
	  animation_speed(animation_speed / textures_size),
	  counter(0),
	  is_animation_finished_once(false) {}

void LoopedAnimation::Update(const double& delta_time) {
	counter += delta_time;
	if (counter >= animation_speed) {
		current_frame = (current_frame + 1) % textures_size;
		counter = 0;
		if (!is_animation_finished_once && current_frame == textures_size - 1) {
			is_animation_finished_once = true;
		}
	}
}

void LoopedAnimation::Reset() {
	current_frame = 0;
	counter = 0;
}

int LoopedAnimation::GetCurrentFrame() const {
	return textures[current_frame];
}

bool LoopedAnimation::IsAnimationFinishedOnce() const {
	return is_animation_finished_once;
}

}  // namespace wolfenstein
