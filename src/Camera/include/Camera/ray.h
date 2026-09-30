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
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace karakale {

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
	// A door slid part open shows its texture from this far along
	double texture_shift;
	// The enemy the ray hits, for the crosshair ray
	ObjectId object_id;

	bool is_hit;
	bool is_hit_vertical;
};

using RayVector = std::vector<Ray>;

// The face of its cell a wall ray struck, by the side it came from: 0 west,
// 1 east, 2 north, 3 south
inline std::uint8_t HitFace(const Ray& ray) {
	if (ray.is_hit_vertical) {
		return ray.direction.x > 0 ? 0 : 1;
	}
	return ray.direction.y > 0 ? 2 : 3;
}

// The wall cell a ray struck: just past its hit point, along it
inline std::pair<int, int> HitCell(const Ray& ray) {
	constexpr double kPast = 1e-4;
	const vector2d inside = ray.hit_point + ray.direction * kPast;
	return {static_cast<int>(std::floor(inside.x)),
			static_cast<int>(std::floor(inside.y))};
}

}  // namespace karakale

#endif	// CAMERA_INCLUDE_CAMERA_RAY_H_