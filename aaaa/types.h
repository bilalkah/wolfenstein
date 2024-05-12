/**
 * @file types.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-03-14
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef TYPES_H_
#define TYPES_H_

#include <cmath>
#include <cstdint>
#include <iostream>

namespace wolfenstein {

struct vector2i {
  int16_t x;
  int16_t y;
};

struct vector2d {
  double x;
  double y;

  vector2d Norm() {
    double size = std::sqrt(x * x + y * y);
    x /= size;
    y /= size;
    return *this;
  }

  vector2d operator*(double scalar) const { return {x * scalar, y * scalar}; }
  vector2d operator+(const vector2d& v) const { return {x + v.x, y + v.y}; }

  friend std::ostream& operator<<(std::ostream& out, const vector2d& v) {
    out << "x: " << v.x << " y: " << v.y;
    return out;
  }
};

struct Position2D {
  Position2D() : pose{0, 0}, theta(0) {}
  Position2D(double x, double y, double theta) : pose{x, y}, theta(theta) {}
  Position2D(vector2d pose, double theta) : pose{pose}, theta(theta) {}

  vector2d pose;
  double theta;

  friend std::ostream& operator<<(std::ostream& out, const Position2D& p) {
    out << "Pose: " << p.pose << " theta: " << p.theta;
    return out;
  }
};

struct Ray {
  Ray() : direction{}, length{} {}
  Ray(vector2d direction_, double length_ = 0.0)
      : direction{direction_}, length{length_} {}

  vector2d direction;
  double length;
  double wall_x;
  double wall_y;
  double perp_wall_dist;
  bool is_x_side;
  uint16_t wall_color;
  double angle;
};

}  // namespace wolfenstein

#endif  // TYPES_H_
