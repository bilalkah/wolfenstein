/**
 * @file raycaster.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_INCLUDE_CAMERA_RAYCASTER_H_
#define CAMERA_INCLUDE_CAMERA_RAYCASTER_H_

#include "Camera/ray.h"
#include "Characters/character.h"
#include "GameMap/map.h"
#include "Math/vector.h"

namespace karakale {

// Casts one ray from `from` at angle `theta` until it hits a wall or has
// travelled `depth`: what lies in a line of fire, or a line of view
Ray CastRay(const Map& map, const Position2D& from, double theta, double depth);

// Casts the camera's fan of rays through the map (DDA), one ray per screen
// column pair. The rays pass through evenly spaced points on a flat camera
// plane in front of the eye, as a flat screen shows the world: a straight
// wall stays straight across the view, however wide the view is.
class RayCaster
{
  public:
	RayCaster(int num_ray, double fov, double depth);

	void Update(const Map& map, const Position2D& position,
				RayVector& rays) const;
	// Where a direction `camera_angle` off the view's centre (negative to
	// the left) crosses the camera plane: -1 at the view's left edge, 1 at
	// its right. The rays are spread by it, and sprites placed by it.
	double Across(double camera_angle) const;

  private:
	double depth_;
	int num_ray_;
	double half_width_;	 // tan(fov / 2): the plane's half width a unit ahead
};

}  // namespace karakale

#endif	// CAMERA_INCLUDE_CAMERA_RAYCASTER_H_
