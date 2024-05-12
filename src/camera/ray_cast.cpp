#include "camera/ray_cast.h"
#include "base/types.h"
#include <cmath>

namespace wolfenstein {

Ray::Ray()
	: origin{0, 0},
	  direction{0, 0},
	  theta(0),
	  is_hit(false),
	  distance(0),
	  hit_point{0, 0},
	  is_hit_vertical(false),
	  perpendicular_distance(0),
	  wall_id(0) {}

Ray::Ray(vector2d direction_, double theta_)
	: origin{0, 0},
	  direction(direction_),
	  theta(theta_),
	  is_hit(false),
	  distance(0),
	  hit_point{0, 0},
	  is_hit_vertical(false),
	  perpendicular_distance(0),
	  wall_id(0) {}

Ray::~Ray() {}

void Ray::Reset(const vector2d ray_orig, const double ray_theta) {
	origin = ray_orig;
	direction = {std::cos(ray_theta), std::sin(ray_theta)};
	direction.Norm();
	theta = ray_theta;
	is_hit = false;
	distance = 0;
	hit_point = {0, 0};
	is_hit_vertical = false;
	perpendicular_distance = 0;
	wall_id = 0;
}

RayCast::RayCast(const int width, const double fov, const double depth)
	: width_(width), fov_(fov), depth_(depth) {
	delta_theta_ = fov_ / width_;
}

RayCast::~RayCast() {}

void RayCast::Update(const Position2D& position,
					 const std::shared_ptr<RayVector>& rays) {

	double ray_theta = position.theta - (fov_ / 2);

	for (auto& ray : *rays) {
		ray = CastRay(position, ray_theta);
		ray_theta += delta_theta_;
	}
}

void RayCast::SetMap(const std::shared_ptr<Map>& map) {
	map_ptr_ = map;
}

Ray RayCast::CastRay(const Position2D& position, const double ray_theta) {
	Ray ray;
	const auto& map = map_ptr_->GetMap();
	const auto& row_size = map_ptr_->GetSizeX();
	const auto& col_size = map_ptr_->GetSizeY();
	vector2d ray_unit_step, ray_length_1d;
	vector2i step, map_check;
	PrepareRay(position, ray_theta, ray, ray_unit_step, ray_length_1d, step,
			   map_check);

	while (!ray.is_hit && ray.distance < depth_) {
		if (ray_length_1d.x < ray_length_1d.y) {
			ray.is_hit_vertical = true;
			ray.perpendicular_distance = ray_length_1d.x;
			ray.distance = ray_length_1d.x;
			ray_length_1d.x += ray_unit_step.x;
			map_check.x += step.x;
		}
		else {
			ray.is_hit_vertical = false;
			ray.perpendicular_distance = ray_length_1d.y;
			ray.distance = ray_length_1d.y;
			ray_length_1d.y += ray_unit_step.y;
			map_check.y += step.y;
		}

		if (map_check.x >= 0 && map_check.x < row_size && map_check.y >= 0 &&
			map_check.y < col_size) {
			if (map[map_check.x][map_check.y] != 0) {
				ray.is_hit = true;
			}
		}
	}

	if (ray.is_hit) {
		ray.hit_point = ray.origin + ray.direction * ray.distance;
		ray.wall_id = map_ptr_->GetMap()[map_check.x][map_check.y];
	}
	return ray;
}

void RayCast::PrepareRay(const Position2D& position, const double ray_theta,
						 Ray& ray, vector2d& ray_unit_step,
						 vector2d& ray_length_1d, vector2i& step,
						 vector2i& map_check) {
	ray.Reset(position.pose, ray_theta);

	ray_unit_step.x =
		ray.direction.x == 0 ? 1e30 : std::abs(1 / ray.direction.x);
	ray_unit_step.y =
		ray.direction.y == 0 ? 1e30 : std::abs(1 / ray.direction.y);
	map_check.FromVector2d(ray.origin);

	if (ray.direction.x < 0) {
		step.x = -1;
		ray_length_1d.x =
			(ray.origin.x - double(map_check.x)) * ray_unit_step.x;
	}
	else {
		step.x = 1;
		ray_length_1d.x =
			(double(map_check.x + 1) - ray.origin.x) * ray_unit_step.x;
	}

	if (ray.direction.y < 0) {
		step.y = -1;
		ray_length_1d.y =
			(ray.origin.y - double(map_check.y)) * ray_unit_step.y;
	}
	else {
		step.y = 1;
		ray_length_1d.y =
			(double(map_check.y + 1) - ray.origin.y) * ray_unit_step.y;
	}
}

}  // namespace wolfenstein
