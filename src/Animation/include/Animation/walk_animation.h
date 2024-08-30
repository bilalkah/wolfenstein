/**
 * @file state_based_animation.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ANIMATION_INCLUDE_ANIMATION_STATE_BASED_ANIMATION_H_
#define ANIMATION_INCLUDE_ANIMATION_STATE_BASED_ANIMATION_H_

#include "Animation/animation.h"
#include <cstdint>
#include <vector>

namespace wolfenstein {

class WalkAnimation : public IAnimation
{
  public:
	WalkAnimation(const std::vector<uint16_t>& tex_ids, const double step_size);
	~WalkAnimation() = default;

	void Update(const double& delta_time) override;
	void Reset() override;

	int GetCurrentFrame() const override;

  private:
	std::vector<uint16_t> tex_ids;
	int current_frame;
	double step_size;
	double counter;
};

}  // namespace wolfenstein

#endif	// ANIMATION_INCLUDE_ANIMATION_STATE_BASED_ANIMATION_H_
