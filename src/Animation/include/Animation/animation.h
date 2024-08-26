/**
 * @file animation.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ANIMATION_INCLUDE_ANIMATION_ANIMATION_H_
#define ANIMATION_INCLUDE_ANIMATION_ANIMATION_H_

namespace wolfenstein {

class IAnimation
{
  public:
	virtual ~IAnimation() = default;

	virtual void Update(const double& delta_time) = 0;
	virtual void Reset() = 0;
	virtual int GetCurrentFrame() const = 0;
};

}  // namespace wolfenstein

#endif	// ANIMATION_INCLUDE_ANIMATION_ANIMATION_H_
