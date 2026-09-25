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
#include "Map/map.h"
#include "Math/vector.h"

namespace wolfenstein {

// Casts the camera's fan of rays through the map (DDA), one ray per screen
// column pair
class RayCaster
{
  public:
	RayCaster(int num_ray, double fov, double depth);

	void Update(const Map& map, const Position2D& position, RayVector& rays);
	double GetDeltaTheta() const;

  private:
	Ray Cast(Map::CellView cells, const Position2D& position,
			 double ray_theta) const;
	void PrepareRay(const Position2D& position, const double ray_angle,
					Ray& ray, vector2d& ray_unit_step, vector2d& ray_length_1d,
					vector2i& step, vector2i& map_check) const;

	double fov_;
	double depth_;
	double delta_theta_;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_RAYCASTER_H_
