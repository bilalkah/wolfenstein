/**
 * @file character.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-18
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CHARACTERS_INCLUDE_CHARACTER_H
#define CHARACTERS_INCLUDE_CHARACTER_H

#include "GameObjects/game_object.h"

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

struct CharacterConfig
{
	Position2D initial_position;
	double translation_speed;
	double rotation_speed;
};

class ICharacter
{
  public:
	virtual ~ICharacter() = default;

	virtual void SetPosition(Position2D position) = 0;
	virtual Position2D GetPosition() const = 0;

};
}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_CHARACTER_H