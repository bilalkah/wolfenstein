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

#include <cstdint>

namespace wolfenstein {

// Fades an opacity from alpha_start to alpha_end, once per Reset(). It only
// computes the alpha: whoever draws the faded texture applies it, so the
// animation knows nothing about textures.
class TriggeredSingleAnimation
{
  public:
	// Covers the fade in 1 / animation_speed seconds
	explicit TriggeredSingleAnimation(double animation_speed,
									  int alpha_start = 128, int alpha_end = 0);

	void Update(double delta_time);
	void Reset();
	bool IsAnimationFinishedOnce() const;
	// The opacity to draw the texture with now
	std::uint8_t GetAlpha() const;

  private:
	double animation_speed{};
	int alpha_start{};
	int alpha_end{};
	// 0 at the start of the fade, 1 at its end; clamped, so a long frame
	// cannot step past the end value
	double progress_{};
};

}  // namespace wolfenstein
#endif	// ANIMATION_INCLUDE_ANIMATION_TRIGGERED_SINGLE_ANIMATION_H_
