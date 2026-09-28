/**
 * @file effect.h
 * @brief A short-lived sprite in a level: the puff where a shot lands
 */

#ifndef GAME_OBJECTS_INCLUDE_EFFECT_H
#define GAME_OBJECTS_INCLUDE_EFFECT_H

#include "GameObjects/game_object.h"
#include "Math/vector.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace wolfenstein {

// Plays a clip once where it is started, then hides until started again.
// A level keeps a few for its whole life and reuses them, so a shot makes
// or frees nothing.
class Effect : public IGameObject
{
  public:
	// Shows `frames` at `pose`, each for `frame_seconds`, drawn width x
	// height (the art places the puff within that) and raised `elevation`
	// off the floor; restarts it if playing
	void Start(const vector2d& pose, std::span<const std::uint16_t> frames,
			   double frame_seconds, double width, double height,
			   double elevation = 0.0) {
		pose_ = pose;
		frames_ = frames;
		frame_seconds_ = frame_seconds;
		width_ = width;
		height_ = height;
		elevation_ = elevation;
		age_ = 0.0;
	}

	void Update(double delta_time) override {
		if (IsVisible()) {
			age_ += delta_time;
		}
	}
	void SetPose(const vector2d& pose) override { pose_ = pose; }
	ObjectType GetObjectType() const override { return ObjectType::EFFECT; }
	vector2d GetPose() const override { return pose_; }
	int GetTextureId() const override {
		return IsVisible() ? frames_[Frame()] : 0;
	}
	double GetWidth() const override { return width_; }
	double GetHeight() const override { return height_; }
	double GetElevation() const override { return elevation_; }
	bool IsVisible() const override {
		return !frames_.empty() &&
			   age_ < frame_seconds_ * static_cast<double>(frames_.size());
	}

  private:
	std::size_t Frame() const {
		const auto frame = static_cast<std::size_t>(age_ / frame_seconds_);
		return frame < frames_.size() ? frame : frames_.size() - 1;
	}

	vector2d pose_{};
	std::span<const std::uint16_t> frames_;
	double frame_seconds_ = 1.0;
	double width_ = 0.0;
	double height_ = 0.0;
	double elevation_ = 0.0;
	double age_ = 0.0;
};

}  // namespace wolfenstein

#endif	// GAME_OBJECTS_INCLUDE_EFFECT_H
