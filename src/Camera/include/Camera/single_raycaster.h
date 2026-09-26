/**
 * @file single_raycaster.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-11-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_INCLUDE_CAMERA_SINGLE_RAYCASTER_H_
#define CAMERA_INCLUDE_CAMERA_SINGLE_RAYCASTER_H_

#include "Camera/ray.h"
#include "GameMap/map.h"
#include "Math/vector.h"

namespace wolfenstein {

// Line of sight from `from` to `to`: the ray's is_hit is set if it reaches
// the target's cell before any blocked cell. A pure function of the map and
// the two points, so it needs no service object or scene.
Ray CastLineOfSight(const Map& map, const vector2d& from, const vector2d& to);

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_SINGLE_RAYCASTER_H_
