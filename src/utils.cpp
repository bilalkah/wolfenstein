#include "include/utils.h"
#include <cstdint>
#include <vector>
#include "types.h"

namespace wolfenstein {

namespace utility {

double ToRadians(const double degree) {
  return degree * M_PI / 180.0;
}

double ToDegrees(const double radian) {
  return radian * 180.0 / M_PI;
}

std::vector<vector2i> GenerateCirclePoints(vector2i center, int radius,
                                           int num_points) {
  std::vector<vector2i> points;
  const double increment = 2 * M_PI / num_points;

  for (int i = 0; i < num_points; ++i) {
    double angle = i * increment;
    int16_t x = static_cast<int16_t>(center.x + radius * std::cos(angle));
    int16_t y = static_cast<int16_t>(center.y + radius * std::sin(angle));

    points.push_back({x, y});
  }

  return points;
}

vector2i ToVector2i(const vector2d& v) {
  return {static_cast<int16_t>(v.x), static_cast<int16_t>(v.y)};
}

}  // namespace utility
}  // namespace wolfenstein