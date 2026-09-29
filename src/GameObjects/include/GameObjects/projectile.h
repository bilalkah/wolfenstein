/**
 * @file projectile.h
 * @brief A shot that flies (a rocket, a plasma bolt) until it bursts
 */

#ifndef GAME_OBJECTS_INCLUDE_PROJECTILE_H
#define GAME_OBJECTS_INCLUDE_PROJECTILE_H

#include "Animation/looped_animation.h"
#include "GameObjects/game_object.h"
#include "Math/vector.h"
#include "SoundManager/sound_manager.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace wolfenstein {

// A weapon's projectile as config.json describes it. Its art is the clips
// "<name>_flight" (seen from 8 sides if it turns, as a rocket does) and
// "<name>_burst".
struct ProjectileConfig
{
	std::string name;
	double speed{};		  // map units a second
	double radius = 0.1;  // how near a wall or a body it comes, and bursts
	// Its picture in flight, and its burst's
	double width{};
	double height{};
	double burst_width{};
	double burst_height{};
	// How far its blast reaches (0: it has none), and what the blast does at
	// the burst and at its edge, on top of a direct hit's damage
	double splash_radius{};
	std::pair<double, double> splash_damage{};
	std::optional<SoundEffect> burst_sound{};
	// How far (in cells, round walls) enemies hear it burst
	int noise_range{};
};

// A projectile in flight, drawn where it is and seen from the side it is
// seen from. The scene moves it, and bursts it on what it meets. A level
// keeps a few for its whole life and reuses them, so firing makes or frees
// nothing.
class Projectile : public IGameObject
{
  public:
	// How high it flies, its middle above the floor (a wall is 1 high)
	static constexpr double kFlightHeight = 0.4;

	// Sets it flying from `from` along `theta`, doing `damage` to what it
	// strikes, fired by the player in slot `owner` with its weapon
	// `weapon`; `config` outlives the flight, and `flight` plays its clip
	void Launch(const ProjectileConfig& config, const LoopedAnimation& flight,
				const vector2d& from, double theta, double damage,
				std::size_t owner = 0, std::size_t weapon = 0) {
		config_ = &config;
		flight_ = flight;
		pose_ = from;
		previous_ = from;
		theta_ = theta;
		damage_ = damage;
		owner_ = owner;
		weapon_ = weapon;
		flying_ = true;
	}
	// Burst: it is gone until launched again
	void Stop() { flying_ = false; }
	// Moves it on to `pose`
	void MoveTo(const vector2d& pose) { pose_ = pose; }
	// Where a tick starts from: it was drawn here
	void StartTick() { previous_ = pose_; }

	bool IsFlying() const { return flying_; }
	const ProjectileConfig& GetConfig() const { return *config_; }
	double GetTheta() const { return theta_; }
	double GetDamage() const { return damage_; }
	std::size_t Owner() const { return owner_; }
	std::size_t WeaponIndex() const { return weapon_; }
	vector2d GetDirection() const {
		return {std::cos(theta_), std::sin(theta_)};
	}

	void Update(double delta_time) override {
		if (flying_) {
			flight_.Update(delta_time);
		}
	}
	void SetPose(const vector2d& pose) override { pose_ = pose; }
	ObjectType GetObjectType() const override { return ObjectType::PROJECTILE; }
	vector2d GetPose() const override { return pose_; }
	vector2d GetRenderPose(double alpha) const override {
		return previous_ + (pose_ - previous_) * alpha;
	}
	bool IsVisible() const override { return flying_; }
	int GetTextureId() const override { return flight_.GetFrame(0); }
	Appearance SeenFrom(const vector2d& viewer) const override {
		return {.texture_id = flight_.GetFrame(SideSeen(pose_, theta_, viewer)),
				.width = GetWidth(),
				.mirrored = false};
	}
	double GetWidth() const override {
		return config_ != nullptr ? config_->width : 0.0;
	}
	double GetHeight() const override {
		return config_ != nullptr ? config_->height : 0.0;
	}
	double GetElevation() const override {
		return std::max(kFlightHeight - GetHeight() / 2, 0.0);
	}

  private:
	const ProjectileConfig* config_ = nullptr;
	LoopedAnimation flight_;
	vector2d pose_{};
	vector2d previous_{};
	double theta_ = 0.0;
	double damage_ = 0.0;
	std::size_t owner_ = 0;
	std::size_t weapon_ = 0;
	bool flying_ = false;
};

}  // namespace wolfenstein

#endif	// GAME_OBJECTS_INCLUDE_PROJECTILE_H
