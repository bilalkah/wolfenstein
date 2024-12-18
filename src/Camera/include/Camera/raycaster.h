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

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace wolfenstein {

class RayCaster
{
  public:
	RayCaster(const int num_ray, const double fov, const double depth,
			  const bool make_parallel = true);
	~RayCaster();

	void Update(const Map& map_ptr, const Position2D& position,
				RayVector& rays);

	double GetDeltaTheta() const;

  private:
	Ray Cast(const MapRaw& map_, const uint16_t row_size,
			 const uint16_t col_size, const Position2D& position,
			 const double ray_theta);

	void PrepareRay(const Position2D& position, const double ray_angle,
					Ray& ray, vector2d& ray_unit_step, vector2d& ray_length_1d,
					vector2i& step, vector2i& map_check);

	void SequentialCast(const Map& map_ptr, const Position2D& position,
						RayVector& rays);

	void ParallelCast(const Map& map_ptr, const Position2D& position,
					  RayVector& rays);
	void CalculateRaySections(int batchSize);

	int num_ray_;
	double fov_;
	double depth_;
	double delta_theta_;
	const bool make_parallel_;
	std::function<void(const Map&, const Position2D&, RayVector&)>
		cast_function_;
	std::vector<std::thread> thread_container_;
	std::vector<std::pair<int, int>> sections_;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_RAYCASTER_H_