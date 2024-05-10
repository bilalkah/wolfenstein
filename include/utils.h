/**
 * @file utils.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-04
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <cmath>
#include <vector>
#include "types.h"

namespace wolfenstein {

namespace utility {

// ToRadians converts degree to radians
double ToRadians(const double degree);

// ToDegrees converts radians to degree
double ToDegrees(const double radian);

// ToVector2i converts vector2d to vector2i
vector2i ToVector2i(const vector2d& v);

// GenerateCirclePoints generates points on the circumference of a circle
std::vector<vector2i> GenerateCirclePoints(int centerX, int centerY,
                                           int radius);

}  // namespace utility
}  // namespace wolfenstein

#endif  // UTILS_H_