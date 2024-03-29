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
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace wolfenstein
{

using ui16 = uint16_t;
using ui32 = uint32_t;
using ui64 = uint64_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

struct Pose2D
{
  Pose2D() : x_(0), y_(0), theta_(0) {}
  Pose2D(double x, double y, double theta) : x_(x), y_(y), theta_(theta) {}
  double x_;
  double y_;
  double theta_;

  friend std::ostream &
  operator<<(std::ostream &out, const Pose2D &p)
  {
    out << "x: " << p.x_ << " y: " << p.y_ << " theta: " << p.theta_;
    return out;
  }
};

struct Direction
{
  Direction() : x_(0), y_(0) {}
  Direction(i16 x, i16 y) : x_(x), y_(y) {}
  i16 x_;
  i16 y_;
};

enum class Rotation
{
  NONE,
  LEFT,
  RIGHT,
};

struct PolarCoordinates
{
  double length;
  double theta;
};

struct vector2i
{
  int x;
  int y;
};

struct vector2d
{
  double x;
  double y;
  void
  Norm()
  {
    double size = std::sqrt(x * x + y * y);
    x /= size;
    y /= size;
  }
};

struct Ray
{
  Ray() : direction_{}, length_{} {}
  Ray(vector2d direction, double length = 0.0)
      : direction_{ direction }, length_{ length }
  {
  }
  vector2d direction_;
  double length_;
  double perpWallDist_;
  bool is_x_side_;
  ui16 wall_color_;
};

} // namespace wolfenstein

#endif // TYPES_H_
