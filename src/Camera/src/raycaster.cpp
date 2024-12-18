#include "Camera/raycaster.h"

namespace wolfenstein {

RayCaster::RayCaster(const int num_ray, const double fov, const double depth,
					 const bool make_parallel)
	: num_ray_(num_ray),
	  fov_(fov),
	  depth_(depth),
	  delta_theta_(fov_ / num_ray),
	  make_parallel_(make_parallel) {
	using namespace std::placeholders;
	int batch_size = 100;
	if (make_parallel_ && false) {
		CalculateRaySections(batch_size);
		cast_function_ = std::bind(&RayCaster::ParallelCast, this, _1, _2, _3);
	}
	else {
		cast_function_ =
			std::bind(&RayCaster::SequentialCast, this, _1, _2, _3);
	}
}

RayCaster::~RayCaster() {
	for (auto& thread : thread_container_) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

void RayCaster::Update(const Map& map_ptr, const Position2D& position,
					   RayVector& rays) {

	cast_function_(map_ptr, position, rays);
}

void RayCaster::SequentialCast(const Map& map_ptr, const Position2D& position,
							   RayVector& rays) {
	const auto map_ = map_ptr.GetRawMap();
	const auto& row_size = map_ptr.GetSizeX();
	const auto& col_size = map_ptr.GetSizeY();
	double ray_theta = position.theta - (fov_ / 2);
	for (auto& ray : rays) {
		ray = Cast(map_, row_size, col_size, position, ray_theta);
		ray_theta += delta_theta_;
	}
}

/// @note Add parallel feature
void RayCaster::ParallelCast(const Map& map_ptr, const Position2D& position,
							 RayVector& rays) {
	const auto map_ = map_ptr.GetRawMap();
	const auto& row_size = map_ptr.GetSizeX();
	const auto& col_size = map_ptr.GetSizeY();
	double ray_theta = position.theta - (fov_ / 2);

	for (const auto pair : sections_) {

		thread_container_.emplace_back(
			std::thread([this, &map_, &row_size, &col_size, &ray_theta,
						 &position, &rays, &pair]() {
				const auto start = pair.first;
				const auto end = pair.second;
				double theta = ray_theta + (start * delta_theta_);
				for (int i = start; i < end; i++) {
					rays[i] = Cast(map_, row_size, col_size, position, theta);
					theta += delta_theta_;
				}
			}));
	}
}

void RayCaster::CalculateRaySections(int batchSize) {
	int start = 0;
	while (start < num_ray_) {
		int end = std::min(start + batchSize,
						   num_ray_);  // Ensure we don't exceed total rays
		sections_.emplace_back(start, end);
		start = end;
	}
}

Ray RayCaster::Cast(const MapRaw& map_, const uint16_t row_size,
					const uint16_t col_size, const Position2D& position,
					const double ray_theta) {
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
