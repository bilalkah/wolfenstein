/**
 * @file game_object.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Initial class for object in the game
 * @version 0.1
 * @date 2024-07-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GAME_OBJECTS_INCLUDE_GAME_OBJECT_H
#define GAME_OBJECTS_INCLUDE_GAME_OBJECT_H

#include "GameObjects/object_id.h"
#include "Math/vector.h"
#include <cstdint>

namespace wolfenstein {

enum class ObjectType : std::uint8_t {
	STATIC_OBJECT,
	DYNAMIC_OBJECT,
	CHARACTER_PLAYER,
	CHARACTER_ENEMY,
	PICKUP,
	EFFECT	// a puff where a shot lands
};

class IGameObject
{
  public:
	virtual ~IGameObject() = default;

  protected:
	// Copies and moves only through derived classes: copying through the
	// base would slice off the derived part
	IGameObject() = default;
	IGameObject(const IGameObject&) = default;
	IGameObject& operator=(const IGameObject&) = default;
	IGameObject(IGameObject&&) = default;
	IGameObject& operator=(IGameObject&&) = default;

  public:
	virtual void Update(double delta_time) = 0;

	virtual void SetPose(const vector2d& pose) = 0;

	virtual ObjectType GetObjectType() const = 0;
	virtual vector2d GetPose() const = 0;
	// Where to draw the object `alpha` of the way from the previous
	// simulation tick to the latest; objects that move override it
	virtual vector2d GetRenderPose(double /*alpha*/) const { return GetPose(); }
	// False while the object is out of the level (a pickup already taken):
	// it stays in the scene's list but is not drawn
	virtual bool IsVisible() const { return true; }
	// How close another body's edge may come to its centre: 0 for what can
	// be walked through (a pickup, a dead enemy)
	virtual double GetCollisionRadius() const { return 0.0; }
	virtual int GetTextureId() const = 0;
	// How it looks from somewhere: its picture, how wide it is drawn, and
	// whether mirrored
	struct Appearance
	{
		int texture_id = 0;
		double width = 0.0;
		bool mirrored = false;
	};
	// Seen from `viewer`: the same from everywhere, but for what turns (an
	// enemy, seen from its side or its back)
	virtual Appearance SeenFrom(const vector2d& /*viewer*/) const {
		return {.texture_id = GetTextureId(),
				.width = GetWidth(),
				.mirrored = false};
	}
	virtual double GetWidth() const = 0;
	virtual double GetHeight() const = 0;
	// How far above the floor it is drawn (a puff where a shot struck high)
	virtual double GetElevation() const { return 0.0; }

	ObjectId GetId() const { return id_; }
	// Set by the scene when it takes the object in
	void SetId(ObjectId id) { id_ = id; }

  private:
	ObjectId id_ = ObjectId::None;
};
}  // namespace wolfenstein

#endif	// GAME_OBJECTS_INCLUDE_GAME_OBJECT_H
