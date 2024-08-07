#include "Math/vector.h"
#include <cmath>

namespace wolfenstein {

vector2i::vector2i() : x(0), y(0) {}

vector2i::vector2i(int x, int y) : x(x), y(y) {}

vector2i vector2i::FromVector2d(const vector2d& v) {
	x = static_cast<int>(v.x);
	y = static_cast<int>(v.y);
	return *this;
}

vector2i vector2i::operator*(const int scalar) const {
	return {x * scalar, y * scalar};
}

vector2i vector2i::operator/(const int scalar) const {
	return {x / scalar, y / scalar};
}

vector2i vector2i::operator+(const vector2i& v) const {
	return {x + v.x, y + v.y};
}

vector2i vector2i::operator-(const vector2i& v) const {
	return {x - v.x, y - v.y};
}

vector2i& vector2i::operator*=(const int scalar) {
	x *= scalar;
	y *= scalar;
	return *this;
}

vector2i& vector2i::operator/=(const int scalar) {
	x /= scalar;
	y /= scalar;
	return *this;
}

vector2i& vector2i::operator+=(const vector2i& v) {
	x += v.x;
	y += v.y;
	return *this;
}

vector2i& vector2i::operator-=(const vector2i& v) {
	x -= v.x;
	y -= v.y;
	return *this;
}

// vector2d --------------------------------------------------------------------

vector2d::vector2d() : x(0), y(0) {}

vector2d::vector2d(double x_, double y_) : x(x_), y(y_) {}

vector2d vector2d::FromVector2i(const vector2i& v) {
	x = static_cast<double>(v.x);
	y = static_cast<double>(v.y);
	return *this;
}

vector2d vector2d::operator*(const double scalar) const {
	return {x * scalar, y * scalar};
}

vector2d vector2d::operator/(const double scalar) const {
	return {x / scalar, y / scalar};
}

vector2d vector2d::operator+(const vector2d& v) const {
	return {x + v.x, y + v.y};
}

vector2d vector2d::operator-(const vector2d& v) const {
	return {x - v.x, y - v.y};
}

vector2d& vector2d::operator*=(const double scalar) {
	x *= scalar;
	y *= scalar;
	return *this;
}

vector2d& vector2d::operator/=(const double scalar) {
	x /= scalar;
	y /= scalar;
	return *this;
}

vector2d& vector2d::operator+=(const vector2d& v) {
	x += v.x;
	y += v.y;
	return *this;
}

vector2d& vector2d::operator-=(const vector2d& v) {
	x -= v.x;
	y -= v.y;
	return *this;
}

vector2d vector2d::Cross(const vector2d& v) const {
	return {x * v.y - y * v.x, x * v.x + y * v.y};
}

double vector2d::Dot(const vector2d& v) const {
	return x * v.x + y * v.y;
}

void vector2d::Norm() {
	double size = std::sqrt(x * x + y * y);
	x /= size;
	y /= size;
}

double vector2d::Magnitude() const {
	return std::sqrt(x * x + y * y);
}

vector2i ToVector2i(const vector2d& v) {
	return {static_cast<int>(v.x), static_cast<int>(v.y)};
}

vector2d ToVector2d(const vector2i& v) {
	return {static_cast<double>(v.x), static_cast<double>(v.y)};
}

std::ostream& operator<<(std::ostream& out, const vector2i& v) {
	out << "x: " << v.x << " y: " << v.y;
	return out;
}

std::ostream& operator<<(std::ostream& out, const vector2d& v) {
	out << "x: " << v.x << " y: " << v.y;
	return out;
}

double ToRadians(const double degree) {
	return degree * M_PI / 180.0;
}

double ToDegrees(const double radian) {
	return radian * 180.0 / M_PI;
}

double Distance(const vector2d& v1, const vector2d& v2) {
	return std::sqrt(std::pow(v1.x - v2.x, 2) + std::pow(v1.y - v2.y, 2));
}

double Distance(const vector2i& v1, const vector2i& v2) {
	return std::sqrt(std::pow(v1.x - v2.x, 2) + std::pow(v1.y - v2.y, 2));
}

}  // namespace wolfenstein
