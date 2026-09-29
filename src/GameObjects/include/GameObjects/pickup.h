/**
 * @file pickup.h
 * @brief An item lying in a level that the player collects by walking over it
 */

#ifndef GAME_OBJECTS_INCLUDE_PICKUP_H
#define GAME_OBJECTS_INCLUDE_PICKUP_H

#include "GameObjects/game_object.h"
#include "Math/vector.h"
#include <cstddef>
#include <cstdint>

namespace wolfenstein {

// What collecting a pickup gives the player
struct PickupEffect
{
	double health = 0.0;
	// Boxes of ammunition for the weapon carried: how many rounds a box holds
	// is the weapon's
	std::size_t ammo_boxes = 0;
	// Keys, as KeyBit()s: the gold key opens gold-locked doors
	std::uint8_t keys = 0;
	// Weapons, a bit per index in the configuration's list
	std::uint8_t weapons = 0;
	// How much of a box each ammunition box is (a dropped clip, half)
	double box_share = 1.0;
};

// Lies still until taken, then leaves the level: it stays in the scene's
// object list (ids are indices into it), hidden, so taking it allocates or
// frees nothing
class Pickup : public IGameObject
{
  public:
	Pickup(const vector2d& pose, int texture_id, double width, double height,
		   PickupEffect effect);

	void Update(double /*delta_time*/) override {}
	void SetPose(const vector2d& pose) override { pose_ = pose; }
	ObjectType GetObjectType() const override { return ObjectType::PICKUP; }
	vector2d GetPose() const override { return pose_; }
	int GetTextureId() const override { return texture_id_; }
	double GetWidth() const override { return width_; }
	double GetHeight() const override { return height_; }
	bool IsVisible() const override { return !taken_; }

	const PickupEffect& GetEffect() const { return effect_; }
	bool IsTaken() const { return taken_; }
	void Take() { taken_ = true; }
	// Lying where it lay again (a match's pickups come back)
	void Restore() { taken_ = false; }
	// What an enemy carries, and drops where it dies: out of the level
	// (taken, as it were) until then
	void MakeDrop() {
		drop_ = true;
		taken_ = true;
	}
	bool IsDrop() const { return drop_; }
	void DropAt(const vector2d& pose) {
		pose_ = pose;
		taken_ = false;
	}

  private:
	vector2d pose_;
	int texture_id_;
	double width_;
	double height_;
	PickupEffect effect_;
	bool taken_ = false;
	bool drop_ = false;
};

}  // namespace wolfenstein

#endif	// GAME_OBJECTS_INCLUDE_PICKUP_H
