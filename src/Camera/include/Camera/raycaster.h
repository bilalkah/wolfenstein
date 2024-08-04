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

#ifndef CAMERA_INCLUDE_RAYCASTER_H_
#define CAMERA_INCLUDE_RAYCASTER_H_

#include "Camera/ray.h"
#include <Characters/character.h>
#include <Map/map.h>
#include <Math/vector.h>
#include <memory>

namespace wolfenstein {

class RayCaster
{
  public:
	RayCaster(const int width, const double fov, const double depth);
	~RayCaster();

	void Update(const std::shared_ptr<Map>& map_ptr, const Position2D& position,
				const std::shared_ptr<RayVector>& rays);

	Ray Cast(const std::shared_ptr<Map>& map_ptr, const Position2D& position,
			 const double ray_theta);

  private:
	void PrepareRay(const Position2D& position, const double ray_angle,
					Ray& ray, vector2d& ray_unit_step, vector2d& ray_length_1d,
					vector2i& step, vector2i& map_check);

	int width_;
	double fov_;
	double depth_;
	double delta_theta_;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_RAYCASTER_H_