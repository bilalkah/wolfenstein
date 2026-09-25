/**
 * @file animation.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ANIMATION_INCLUDE_ANIMATION_TRIGGERED_SINGLE_ANIMATION_H_
#define ANIMATION_INCLUDE_ANIMATION_TRIGGERED_SINGLE_ANIMATION_H_

#include "Animation/animation.h"
#include <cstdint>

namespace wolfenstein {

// Fades one texture's opacity from alpha_start to alpha_end, once per
// Reset(). It only computes the alpha: whoever draws the texture applies it,
// so the animation needs no access to textures.
class TriggeredSingleAnimation : public IAnimation
{
  public:
	// Covers the fade in 1 / animation_speed seconds
	TriggeredSingleAnimation(const uint16_t texture_id,
							 const double animation_speed,
							 int alpha_start = 128, int alpha_end = 0);

	void Update(const double& delta_time) override;
	void Reset() override;

	int GetCurrentFrame() const override;
	bool IsAnimationFinishedOnce() const override;
	// The opacity to draw the texture with now
	std::uint8_t GetAlpha() const;

  private:
	uint16_t texture_id{};
	double animation_speed{};
	int alpha_start{};
	int alpha_end{};
	// 0 at the start of the fade, 1 at its end; clamped, so a long frame
	// cannot step past the end value
	double progress_{};
};

}  // namespace wolfenstein
#endif	// ANIMATION_INCLUDE_ANIMATION_TRIGGERED_SINGLE_ANIMATION_H_
