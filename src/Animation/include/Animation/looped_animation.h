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
#include <span>
#include <string_view>

namespace wolfenstein {

class TextureManager;

// Cycles through the frames of a clip. The frame ids are a view into the
// clip TextureManager keeps for the program's life, shared by every animation
// playing it, so building one allocates nothing and owners hold it by value.
class LoopedAnimation : public IAnimation
{
  public:
	// Plays nothing; assign a real animation before use
	LoopedAnimation() = default;
	// Shows each frame for frame_seconds
	LoopedAnimation(std::span<const std::uint16_t> frames,
					double frame_seconds);
	// Plays the named clip ("green_light") once every cycle_seconds
	LoopedAnimation(const TextureManager& textures, std::string_view clip,
					double cycle_seconds);
	// Plays the clip "<owner>_<clip>" ("soldier" and "walk") once every
	// cycle_seconds
	LoopedAnimation(const TextureManager& textures, std::string_view owner,
					std::string_view clip, double cycle_seconds);

	// The frames of the clip "<owner>_<clip>", found without allocating
	static std::span<const std::uint16_t> Clip(const TextureManager& textures,
											   std::string_view owner,
											   std::string_view clip);

	void Update(const double& delta_time) override;
	void Reset() override;
	int GetCurrentFrame() const override;
	bool IsAnimationFinishedOnce() const override;

  private:
	std::span<const std::uint16_t> frames_;
	double frame_seconds_ = 0.0;
	double counter_ = 0.0;
	std::size_t current_frame_ = 0;
	bool finished_once_ = false;
};

}  // namespace wolfenstein

#endif	// ANIMATION_INCLUDE_ANIMATION_LOOPED_ANIMATION_H_
