/**
 * @file i_character.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Character is an interface for renderable objects
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CHARACTER_I_CHARACTER_H
#define CHARACTER_I_CHARACTER_H

#include "base/types.h"

namespace wolfenstein {

struct Position2D
{
	Position2D() : pose{0, 0}, theta(0) {}
	Position2D(vector2d pose, double theta) : pose{pose}, theta(theta) {}
	Position2D(const Position2D& other)
		: pose{other.pose}, theta(other.theta) {}

	vector2d pose;
	double theta;
};

struct PlayerConfig
{
	Position2D initial_pose;
	double translation_speed;
	double rotation_speed;
};

class ICharacter
{
  public:
	virtual ~ICharacter() = default;
	virtual void Update(const double) = 0;
	virtual Position2D GetPosition() const = 0;
};

}  // namespace wolfenstein

#endif	// CHARACTER_I_CHARACTER_H
