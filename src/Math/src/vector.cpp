#include <Math/vector.h>
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

double vector2d::Magnitude() const {
	return std::sqrt(x * x + y * y);
}

double vector2d::Determinant(const vector2d& v) const {
	return x * v.y - y * v.x;
}

double vector2d::Distance(const vector2d& v) const {
	return std::sqrt(std::pow(x - v.x, 2) + std::pow(y - v.y, 2));
}

void vector2d::Norm() {
	double size = std::sqrt(x * x + y * y);
	x /= size;
	y /= size;
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

double SumRadian(const double radian1, const double radian2) {
	auto angle = std::fmod(radian1 + radian2, (M_PI * 2));
	if (angle > M_PI) {
		angle -= 2 * M_PI;
	}
	else if (angle < -M_PI) {
		angle += 2 * M_PI;
	}
	return angle;
}

double SubRadian(const double degree1, const double degree2) {
	auto angle = std::fmod(degree1 - degree2, (M_PI * 2));
	if (angle > M_PI) {
		angle -= 2 * M_PI;
	}
	else if (angle < -M_PI) {
		angle += 2 * M_PI;
	}
	return angle;
}

double SumDegree(const double degree1, const double degree2) {
	auto angle = std::fmod(degree1 + degree2, 360.0);
	if (angle > 180.0) {
		angle -= 2 * 180.0;
	}
	else if (angle < -180.0) {
		angle += 2 * 180.0;
	}
	return angle;
}

double SubDegree(const double degree1, const double degree2) {
	auto angle = std::fmod(degree1 - degree2, 360.0);
	if (angle > 180.0) {
		angle -= 2 * 180.0;
	}
	else if (angle < -180.0) {
		angle += 2 * 180.0;
	}
	return angle;
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

double CalculateAngleBetweenTwoVectors(const vector2d& v1, const vector2d& v3) {
	return std::acos(v1.Dot(v3) / (v1.Magnitude() * v3.Magnitude()));
}

double CalculateAngleBetweenTwoVectorsSigned(const vector2d& v1,
											 const vector2d& v3) {
	auto angle = std::acos(v1.Dot(v3) / (v1.Magnitude() * v3.Magnitude()));

	const vector2d v2;
	double orientation_val =
		(v2.y - v1.y) * (v3.x - v2.x) - (v2.x - v1.x) * (v3.y - v2.y);
	if (orientation_val > 0.0) {
		angle = -angle;
	}
	return angle;
}

double CalculateAngleBetweenThreeVectorsSigned(const vector2d& v1,
											   const vector2d& v2,
											   const vector2d& v3) {
	const auto v1_base = v1 - v2;
	const auto v3_base = v3 - v2;
	auto angle = std::acos(v1_base.Dot(v3_base) /
						   (v1_base.Magnitude() * v3_base.Magnitude()));

	double orientation_val =
		(v2.y - v1.y) * (v3.x - v2.x) - (v2.x - v1.x) * (v3.y - v2.y);
	if (orientation_val > 0.0) {
		angle = -angle;
	}
	return angle;
}

}  // namespace wolfenstein
