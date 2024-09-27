#include "Animation/time_based_single_animation.h"
#include "TextureManager/texture_manager.h"
namespace wolfenstein {

TBSAnimation::TBSAnimation(const std::vector<uint16_t>& textures,
						   const double animation_speed)
	: textures(textures),
	  current_frame(0),
	  animation_speed(animation_speed),
	  counter(0) {}

TBSAnimation::TBSAnimation(const std::string collection_name,
						   const double animation_speed)
	: textures(
		  TextureManager::GetInstance().GetTextureCollection(collection_name)),
	  current_frame(0),
	  animation_speed(animation_speed / textures.size()),
	  counter(0) {}

void TBSAnimation::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
}

void TBSAnimation::Reset() {
	current_frame = 0;
	counter = 0;
}

int TBSAnimation::GetCurrentFrame() const {
	return textures[current_frame];
}

}  // namespace wolfenstein
