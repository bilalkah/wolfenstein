#include "Camera/raycaster.h"
#include <cmath>
#include <cstddef>

namespace wolfenstein {

RayCaster::RayCaster(int num_ray, double fov, double depth)
	: fov_(fov), depth_(depth), delta_theta_(fov_ / num_ray) {}

namespace {

void PrepareRay(const Position2D& position, const double ray_theta, Ray& ray,
				vector2d& ray_unit_step, vector2d& ray_length_1d,
				vector2i& step, vector2i& map_check);

// DDA through the cells from `position` at `ray_theta`, until a wall or
// `depth`
Ray Cast(Map::CellView cells, const Position2D& position, double ray_theta,
		 double depth) {
	const auto row_size = static_cast<int>(cells.extent(0));
	const auto col_size = static_cast<int>(cells.extent(1));
	Ray ray;
	vector2d ray_unit_step, ray_length_1d;
	vector2i step, map_check;
	PrepareRay(position, ray_theta, ray, ray_unit_step, ray_length_1d, step,
			   map_check);

	while (!ray.is_hit && ray.distance < depth) {
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
			const auto cell = cells[static_cast<std::size_t>(map_check.x),
									static_cast<std::size_t>(map_check.y)];
			if (cell != 0) {
				ray.is_hit = true;
				ray.hit_point = ray.origin + ray.direction * ray.distance;
				ray.wall_id = cell;
			}
		}
	}
	return ray;
}

void PrepareRay(const Position2D& position, const double ray_theta, Ray& ray,
				vector2d& ray_unit_step, vector2d& ray_length_1d,
				vector2i& step, vector2i& map_check) {

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

}  // namespace

void RayCaster::Update(const Map& map, const Position2D& position,
					   RayVector& rays) const {
	const auto cells = map.GetCells();
	double ray_theta = position.theta - (fov_ / 2);
	for (auto& ray : rays) {
		ray = Cast(cells, position, ray_theta, depth_);
		ray_theta += delta_theta_;
	}
}

Ray CastRay(const Map& map, const Position2D& from, double theta,
			double depth) {
	return Cast(map.GetCells(), from, theta, depth);
}

double RayCaster::GetDeltaTheta() const {
	return delta_theta_;
}

}  // namespace wolfenstein
