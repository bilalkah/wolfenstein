/**
 * @file utils.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Utilities for rendering
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include "base/types.h"
#include <SDL2/SDL.h>
#include <vector>

namespace wolfenstein {

// GenerateCirclePoints generates points on the circumference of a circle
std::vector<vector2i> GenerateCirclePoints(vector2i center, int radius,
										   int num_points);

}  // namespace wolfenstein

#endif	// RENDER_UTILS_H
