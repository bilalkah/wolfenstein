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

#ifndef GAME_OBJECTS_GAME_OBJECT_H
#define GAME_OBJECTS_GAME_OBJECT_H

#include <Math/vector.h>

namespace wolfenstein {

enum class ObjectType {
	STATIC_OBJECT,
	DYNAMIC_OBJECT,
	CHARACTER_PLAYER,
	CHARACTER_ENEMY
};

class IGameObject
{
  public:
	virtual ~IGameObject() = default;

	virtual void Update(double delta_time) = 0;

	virtual void SetPose(const vector2d& pose) = 0;
	virtual ObjectType GetObjectType() const = 0;
	virtual vector2d GetPose() const = 0;
};
}  // namespace wolfenstein

#endif	// GAME_OBJECTS_GAME_OBJECT_H
