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

namespace wolfenstein {

// Casts one ray from `from` at angle `theta` until it hits a wall or has
// travelled `depth`: what lies in a line of fire, or a line of view
Ray CastRay(const Map& map, const Position2D& from, double theta, double depth);

// Casts the camera's fan of rays through the map (DDA), one ray per screen
// column pair
class RayCaster
{
  public:
	RayCaster(int num_ray, double fov, double depth);

	void Update(const Map& map, const Position2D& position, RayVector& rays);
	double GetDeltaTheta() const;

  private:
	double fov_;
	double depth_;
	double delta_theta_;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_RAYCASTER_H_
