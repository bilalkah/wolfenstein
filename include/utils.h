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

namespace wolfenstein {

namespace utility {

// ToRadians converts degree to radians
double ToRadians(const double degree) {
  return degree * M_PI / 180.0;
}

// ToDegrees converts radians to degree
double ToDegrees(const double radian) {
  return radian * 180.0 / M_PI;
}

}  // namespace utility
}  // namespace wolfenstein

#endif  // UTILS_H_