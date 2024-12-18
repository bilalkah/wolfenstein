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

#include "Math/vector.h"

namespace wolfenstein {

struct Position2D
{
	Position2D() : pose{0, 0}, theta(0) {}
	Position2D(vector2d pose, double theta) : pose{pose}, theta(theta) {}
	Position2D(const Position2D& other)
		: pose{other.pose}, theta(other.theta) {}
	Position2D& operator=(const Position2D& other) {
		if (this != &other) {
			pose = other.pose;
			theta = other.theta;
		}
		return *this;
	}

	vector2d pose;
	double theta;
};

struct CharacterConfig
{
	CharacterConfig(Position2D initial_position, double translation_speed,
					double rotation_speed, double width, double height)
		: initial_position(initial_position),
		  translation_speed(translation_speed),
		  rotation_speed(rotation_speed),
		  width(width),
		  height(height) {}
	Position2D initial_position;
	double translation_speed;
	double rotation_speed;
	double width;
	double height;
};

class ICharacter
{
  public:
	virtual ~ICharacter() = default;

	virtual void SetPosition(const Position2D position) = 0;
	virtual Position2D GetPosition() const = 0;
	virtual void IncreaseHealth(double amount) = 0;
	virtual void DecreaseHealth(double amount) = 0;
	virtual double GetHealth() const = 0;
};
}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_CHARACTER_H