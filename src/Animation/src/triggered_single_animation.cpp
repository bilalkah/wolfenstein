#include "Animation/triggered_single_animation.h"
#include "TextureManager/texture_manager.h"
#include <cassert>

#define assertm(exp, msg) assert((void(msg), exp))

namespace wolfenstein {

TriggeredSingleAnimation::TriggeredSingleAnimation(const uint16_t texture_id,
												   const double animation_speed,
												   int alpha_start,
												   int alpha_end)
	: texture(TextureManager::GetInstance().GetTexture(texture_id)),
	  texture_id(texture_id),
	  counter(0),
	  animation_speed(animation_speed),
	  transparancy(alpha_start),
	  alpha_start(alpha_start),
	  alpha_end(alpha_end),
	  direction((alpha_end - alpha_start) / abs((alpha_end - alpha_start))),
	  is_animation_finished_once(false) {}

void TriggeredSingleAnimation::Update(const double& delta_time) {
	if (is_animation_finished_once) {
		return;
	}
	counter += delta_time;
	transparancy =
		static_cast<int>(alpha_start + (counter * direction * animation_speed) *
										   abs(alpha_start - alpha_end));
	SDL_SetTextureAlphaMod(texture.texture, transparancy);

	if (!is_animation_finished_once && transparancy == alpha_end) {
		is_animation_finished_once = true;
	}
}  // namespace wolfenstein

void TriggeredSingleAnimation::Reset() {
	counter = 0;
	transparancy = alpha_start;
	is_animation_finished_once = false;
}

int TriggeredSingleAnimation::GetCurrentFrame() const {
	return texture_id;
}

bool TriggeredSingleAnimation::IsAnimationFinishedOnce() const {
	return is_animation_finished_once;
}

}  // namespace wolfenstein
