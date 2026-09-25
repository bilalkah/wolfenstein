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
#include <cmath>
#include <numbers>

namespace wolfenstein {

struct Position2D
{
	Position2D() : pose{0, 0}, theta(0) {}
	Position2D(vector2d pose, double theta) : pose{pose}, theta(theta) {}

	vector2d pose;
	double theta;
};

// The state `alpha` of the way from `from` to `to` (0 to 1), turning the
// short way round: what is drawn between two simulation ticks
inline vector2d Interpolate(const vector2d& from, const vector2d& to,
							double alpha) {
	return from + (to - from) * alpha;
}
inline Position2D Interpolate(const Position2D& from, const Position2D& to,
							  double alpha) {
	const double turn =
		std::remainder(to.theta - from.theta, 2.0 * std::numbers::pi);
	return {Interpolate(from.pose, to.pose, alpha), from.theta + turn * alpha};
}

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

  protected:
	// Copies and moves only through derived classes: copying through the
	// base would slice off the derived part
	ICharacter() = default;
	ICharacter(const ICharacter&) = default;
	ICharacter& operator=(const ICharacter&) = default;
	ICharacter(ICharacter&&) = default;
	ICharacter& operator=(ICharacter&&) = default;

  public:
	virtual void SetPosition(const Position2D position) = 0;
	virtual const Position2D& GetPosition() const = 0;
	virtual void IncreaseHealth(double amount) = 0;
	virtual void DecreaseHealth(double amount) = 0;
	virtual double GetHealth() const = 0;
};
}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_CHARACTER_H