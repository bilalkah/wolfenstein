/**
 * @file types.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Base types for the project
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BASE_TYPES_H_
#define BASE_TYPES_H_

#include <iostream>

namespace wolfenstein {

struct vector2d;

struct vector2i
{
	int x;
	int y;

	vector2i();
	vector2i(int x, int y);
	vector2i FromVector2d(const vector2d& v);
	vector2i operator*(const int scalar) const;
	vector2i operator/(const int scalar) const;
	vector2i operator+(const vector2i& v) const;
	vector2i operator-(const vector2i& v) const;
	vector2i& operator*=(const int scalar);
	vector2i& operator/=(const int scalar);
	vector2i& operator+=(const vector2i& v);
	vector2i& operator-=(const vector2i& v);
};

struct vector2d
{
	double x;
	double y;

	vector2d();
	vector2d(double x, double y);
	vector2d FromVector2i(const vector2i& v);
	vector2d operator*(const double scalar) const;
	vector2d operator/(const double scalar) const;
	vector2d operator+(const vector2d& v) const;
	vector2d operator-(const vector2d& v) const;
	vector2d& operator*=(const double scalar);
	vector2d& operator/=(const double scalar);
	vector2d& operator+=(const vector2d& v);
	vector2d& operator-=(const vector2d& v);

	vector2d Cross(const vector2d& v) const;
	double Dot(const vector2d& v) const;
	void Norm();
	double Magnitude() const;
};

// To convert vector2d to vector2i
vector2i ToVector2i(const vector2d& v);

// To convert vector2i to vector2d
vector2d ToVector2d(const vector2i& v);

// ostream operator for vector2d
std::ostream& operator<<(std::ostream& out, const vector2i& v);

// ostream operator for vector2i
std::ostream& operator<<(std::ostream& out, const vector2d& v);

}  // namespace wolfenstein

#endif	// BASE_TYPES_H_
