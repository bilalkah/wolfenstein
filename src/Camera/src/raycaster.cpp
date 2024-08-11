#include "Camera/raycaster.h"
#include <cmath>
#include <vector>

namespace wolfenstein {

RayCaster::RayCaster(const int num_ray, const double fov, const double depth)
	: num_ray_(num_ray),
	  fov_(fov),
	  depth_(depth),
	  delta_theta_(fov_ / num_ray) {}

RayCaster::~RayCaster() {}

void RayCaster::Update(const std::shared_ptr<Map>& map_ptr,
					   const Position2D& position,
					   const std::shared_ptr<RayVector>& rays) {

	double ray_theta = position.theta - (fov_ / 2);
	for (auto& ray : *rays) {
		ray = Cast(map_ptr, position, ray_theta);
		ray_theta += delta_theta_;
	}
}

Ray RayCaster::Cast(const std::shared_ptr<Map>& map_ptr,
					const Position2D& position, const double ray_theta) {
	const auto map_ = map_ptr->GetMap();
	const auto& row_size = map_ptr->GetSizeX();
	const auto& col_size = map_ptr->GetSizeY();
	Ray ray;
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
			if (map_[map_check.x][map_check.y] != 0) {
				ray.is_hit = true;
				ray.hit_point = ray.origin + ray.direction * ray.distance;
				ray.wall_id = map_[map_check.x][map_check.y];
			}
		}
	}
	return ray;
}

double RayCaster::GetDeltaTheta() const {
	return delta_theta_;
}

void RayCaster::PrepareRay(const Position2D& position, const double ray_theta,
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
