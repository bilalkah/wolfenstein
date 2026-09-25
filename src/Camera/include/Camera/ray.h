/**
 * @file ray.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-21
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_INCLUDE_CAMERA_RAY_H_
#define CAMERA_INCLUDE_CAMERA_RAY_H_

#include "GameObjects/object_id.h"
#include "Math/vector.h"
#include <vector>

namespace wolfenstein {

struct Ray
{
	Ray();
	Ray(vector2d direction, double theta);

	void Reset(const vector2d ray_orig, const double ray_theta);

	vector2d origin;
	vector2d direction;
	vector2d hit_point;

	double theta;
	double distance;
	double perpendicular_distance;

	int wall_id;
	// The enemy the ray hits, for the crosshair ray
	ObjectId object_id;

	bool is_hit;
	bool is_hit_vertical;
};

typedef std::vector<Ray> RayVector;

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_RAY_H_