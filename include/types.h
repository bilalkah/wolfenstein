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

struct Pose2D {
  Pose2D() : x_(0), y_(0), theta_(0) {}
  Pose2D(double x, double y, double theta) : x_(x), y_(y), theta_(theta) {}
  double x_;
  double y_;
  double theta_;

  friend std::ostream& operator<<(std::ostream& out, const Pose2D& p) {
    out << "x: " << p.x_ << " y: " << p.y_ << " theta: " << p.theta_;
    return out;
  }
};

struct Direction {
  Direction() : x_(0), y_(0) {}
  Direction(int16_t x, int16_t y) : x_(x), y_(y) {}
  int16_t x_;
  int16_t y_;
};

struct vector2i {
  int x;
  int y;
};

struct vector2d {
  double x;
  double y;

  void Norm() {
    double size = std::sqrt(x * x + y * y);
    x /= size;
    y /= size;
  }

  vector2d operator*(double scalar) const { return {x * scalar, y * scalar}; }
};

struct Ray {
  Ray() : direction_{}, length_{} {}
  Ray(vector2d direction, double length = 0.0)
      : direction_{direction}, length_{length} {}
  vector2d direction_;
  double length_;
  double perp_wall_dist_;
  bool is_x_side_;
  uint16_t wall_color_;
  double angle_;
};

}  // namespace wolfenstein

#endif  // TYPES_H_
