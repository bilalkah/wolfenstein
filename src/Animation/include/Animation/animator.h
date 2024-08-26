/**
 * @file animator.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ANIMATION_INCLUDE_ANIMATION_ANIMATOR_H_
#define ANIMATION_INCLUDE_ANIMATION_ANIMATOR_H_

#include "Animation/animation.h"
#include <memory>
#include <vector>

namespace wolfenstein {

class Animator
{
  public:
	Animator();
	~Animator() = default;

	void AddAnimation(std::shared_ptr<IAnimation> animation);
	void Update(const double& delta_time);
	void Reset();
	int GetCurrentFrame() const;

  private:
	std::vector<std::shared_ptr<IAnimation>> animations;
};

}  // namespace wolfenstein

#endif	// ANIMATION_INCLUDE_ANIMATION_ANIMATOR_H_
