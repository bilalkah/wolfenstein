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

#ifndef ANIMATION_INCLUDE_ANIMATION_TIME_BASED_SINGLE_ANIMATION_H_
#define ANIMATION_INCLUDE_ANIMATION_TIME_BASED_SINGLE_ANIMATION_H_

#include "Animation/animation.h"
#include <vector>

namespace wolfenstein {

class TBSAnimation : public IAnimation
{
  public:
	TBSAnimation(const std::vector<int>& tex_ids, const double animation_speed);
	~TBSAnimation() = default;

	void Update(const double& delta_time) override;
	void Reset() override;

	int GetCurrentFrame() const override;

  private:
	std::vector<int> tex_ids;
	int current_frame;
	double animation_speed;
	double counter;
};

}  // namespace wolfenstein
#endif	// ANIMATION_INCLUDE_ANIMATION_TIME_BASED_SINGLE_ANIMATION_H_
