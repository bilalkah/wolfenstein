/**
 * @file ray_cast.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_RAY_CAST_H
#define CAMERA_RAY_CAST_H

#include "base/types.h"
#include "character/i_character.h"
#include "map/map.h"
#include <memory>
#include <vector>

namespace wolfenstein {

struct Ray
{
	Ray();
	Ray(vector2d direction, double theta);
	~Ray();

	void Reset(const vector2d ray_orig, const double ray_theta);
	vector2d origin;
	vector2d direction;
	double theta;
	bool is_hit;
	double distance;
	vector2d hit_point;
	bool is_hit_vertical;
	double perpendicular_distance;
	int wall_id;
};

typedef std::vector<Ray> RayVector;

class RayCast
{
  public:
	RayCast(const int width, const double fov, const double depth);
	~RayCast();

	void Update(const Position2D& position,
				const std::shared_ptr<RayVector>& rays_);
	void SetMap(const std::shared_ptr<Map>& map);

	Ray CastRay(const Position2D& position, const double ray_angle,
				const std::vector<std::vector<uint16_t>>& map,
				const int row_size, const int col_size);

  private:
	void PrepareRay(const Position2D& position, const double ray_angle,
					Ray& ray, vector2d& ray_unit_step, vector2d& ray_length_1d,
					vector2i& step, vector2i& map_check);

	int width_;
	double fov_;
	double depth_;
	double delta_theta_;
	std::shared_ptr<Map> map_ptr_;
};

}  // namespace wolfenstein

#endif	// CAMERA_RAY_CAST_H