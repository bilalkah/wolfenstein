/**
 * @file vector.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef MATH_INCLUDE_MATH_VECTOR_H_
#define MATH_INCLUDE_MATH_VECTOR_H_

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
	double Magnitude() const;
	double Determinant(const vector2d& v) const;
	double Distance(const vector2d& v) const;
	void Norm();
};

// To convert vector2d to vector2i
vector2i ToVector2i(const vector2d& v);

// To convert vector2i to vector2d
vector2d ToVector2d(const vector2i& v);

// ostream operator for vector2d
std::ostream& operator<<(std::ostream& out, const vector2i& v);

// ostream operator for vector2i
std::ostream& operator<<(std::ostream& out, const vector2d& v);

// SumRadian calculates the sum of two radian values
double SumRadian(const double radian1, const double radian2);

// SubRadian calculates the subtraction of two radian values
double SubRadian(const double radian1, const double radian2);

// SumDegree calculates the sum of two degree values
double SumDegree(const double degree1, const double degree2);

// SubDegree calculates the subtraction of two degree values
double SubDegree(const double degree1, const double degree2);

// ToRadians converts degree to radians
double ToRadians(const double degree);

// ToDegrees converts radians to degree
double ToDegrees(const double radian);

// Distance calculates the distance between two points
double Distance(const vector2d& v1, const vector2d& v2);

// Distance calculates the distance between two points
double Distance(const vector2i& v1, const vector2i& v2);

// CalculateAngleBetweenTwoVectors calculates the angle between two vectors
double CalculateAngleBetweenTwoVectors(const vector2d& v1, const vector2d& v2);

// CalculateAngleBetweenTwoVectorsSigned calculates the signed angle between two vectors
double CalculateAngleBetweenTwoVectorsSigned(const vector2d& v1,
											 const vector2d& v2);

double CalculateAngleBetweenThreeVectorsSigned(const vector2d& v1,
											   const vector2d& v2,
											   const vector2d& v3);

}  // namespace wolfenstein

#endif	// MATH_INCLUDE_MATH_VECTOR_H_
