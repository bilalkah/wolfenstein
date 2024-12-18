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
#include <string>
#include <vector>

namespace wolfenstein {

class Texture;

class TriggeredSingleAnimation : public IAnimation
{
  public:
	TriggeredSingleAnimation(const uint16_t texture_id,
							 const double animation_speed,
							 int alpha_start = 128, int alpha_end = 0);
	~TriggeredSingleAnimation() = default;

	void Update(const double& delta_time) override;
	void Reset() override;

	int GetCurrentFrame() const override;
	bool IsAnimationFinishedOnce() const override;

  private:
	Texture& texture;
	uint16_t texture_id;
	double counter;
	double animation_speed;
	int transparancy;
	int alpha_start;
	int alpha_end;
	int direction;
	bool is_animation_finished_once;
};

}  // namespace wolfenstein
#endif	// ANIMATION_INCLUDE_ANIMATION_TRIGGERED_SINGLE_ANIMATION_H_
