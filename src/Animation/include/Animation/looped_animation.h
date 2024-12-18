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

#ifndef ANIMATION_INCLUDE_ANIMATION_LOOPED_ANIMATION_H_
#define ANIMATION_INCLUDE_ANIMATION_LOOPED_ANIMATION_H_

#include "Animation/animation.h"
#include <cstdint>
#include <string>
#include <vector>

namespace wolfenstein {

class LoopedAnimation : public IAnimation
{
  public:
	LoopedAnimation(const std::vector<uint16_t>& textures,
				 const double animation_speed);
	LoopedAnimation(const std::string collection_name,
				 const double animation_speed);
	~LoopedAnimation() = default;

	void Update(const double& delta_time) override;
	void Reset() override;

	int GetCurrentFrame() const override;
	bool IsAnimationFinishedOnce() const override;

  private:
	std::vector<uint16_t> textures;
	int textures_size;
	int current_frame;
	double animation_speed;
	double counter;
	bool is_animation_finished_once;
};

}  // namespace wolfenstein
#endif	// ANIMATION_INCLUDE_ANIMATION_LOOPED_ANIMATION_H_
