#include "Camera/ray.h"

#include <cmath>

namespace karakale {

Ray::Ray()
	: origin{0, 0},
	  direction{0, 0},
	  hit_point{0, 0},
	  theta(0),
	  distance(0),
	  perpendicular_distance(0),
	  wall_id(0),
	  texture_shift(0),
	  object_id(ObjectId::None),
	  is_hit(false),
	  is_hit_vertical(false) {}

Ray::Ray(vector2d direction_, double theta_)
	: origin{0, 0},
	  direction(direction_),
	  hit_point{0, 0},
	  theta(theta_),
	  distance(0),
	  perpendicular_distance(0),
	  wall_id(0),
	  texture_shift(0),
	  object_id(ObjectId::None),
	  is_hit(false),
	  is_hit_vertical(false) {}

void Ray::Reset(const vector2d ray_orig, const double ray_theta) {
	origin = ray_orig;
	direction = {std::cos(ray_theta), std::sin(ray_theta)};
	direction.Norm();
	hit_point = {0, 0};
	theta = ray_theta;
	distance = 0;
	perpendicular_distance = 0;
	wall_id = 0;
	texture_shift = 0;
	object_id = ObjectId::None;
	is_hit = false;
	is_hit_vertical = false;
}

}  // namespace karakale