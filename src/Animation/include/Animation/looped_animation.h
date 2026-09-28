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
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace wolfenstein {

class TextureManager;

// Cycles through the frames of a clip. The frame ids are a view into the
// clip TextureManager keeps for the program's life, shared by every animation
// playing it, so building one allocates nothing and owners hold it by value.
//
// A clip may be seen from 8 sides (an enemy walking): "<owner>_<clip>" from
// the front, and "<owner>_<clip>@2" to "@8" from the others, going round
// from front-left to front-right (Doom's rotations 2 to 8), each as long.
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
	// Likewise, for a clip the art may leave out: empty if it does
	static std::span<const std::uint16_t> FindClip(
		const TextureManager& textures, std::string_view owner,
		std::string_view clip);

	// Sides a clip may be seen from
	static constexpr std::size_t kViews = 8;

	void Update(const double& delta_time) override;
	void Reset() override;
	int GetCurrentFrame() const override;
	// The current frame seen from `view` (0 in front, round to 7): the
	// front's, for a clip that looks the same from every side
	int GetFrame(std::size_t view) const;
	bool IsAnimationFinishedOnce() const override;

  private:
	std::span<const std::uint16_t> frames_;
	// Each side's frames, as long as the front's; null for a clip seen the
	// same from every side
	std::array<const std::uint16_t*, kViews> views_{};
	double frame_seconds_ = 0.0;
	double counter_ = 0.0;
	std::size_t current_frame_ = 0;
	bool finished_once_ = false;
};

}  // namespace wolfenstein

#endif	// ANIMATION_INCLUDE_ANIMATION_LOOPED_ANIMATION_H_
