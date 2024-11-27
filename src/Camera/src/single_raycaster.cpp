#include "Camera/single_raycaster.h"
#include "Math/vector.h"
#include <cmath>

namespace wolfenstein {

SingleRayCasterService* SingleRayCasterService::instance_ = nullptr;

SingleRayCasterService& SingleRayCasterService::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new SingleRayCasterService();
	}
	return *instance_;
}

void SingleRayCasterService::InitService(const std::shared_ptr<Map>& map_ptr) {
	map_ptr_ = map_ptr;
}

Ray SingleRayCasterService::Cast(const vector2d& src) {

	auto map_ = map_ptr_->GetMap();
	const auto& row_size = map_ptr_->GetSizeX();
	const auto& col_size = map_ptr_->GetSizeY();
	Ray ray;
	vector2d ray_unit_step, ray_length_1d;
	vector2i step, map_check, dest_i;
	PrepareRay(src, ray, ray_unit_step, ray_length_1d, step, map_check);
	auto depth_ = src.Distance(dest);
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
				break;
			}
			else if (ToVector2i(dest) == map_check) {
				ray.is_hit = true;
				ray.hit_point = ray.origin + ray.direction * depth_;
			}
		}
	}
	if (ToVector2i(dest) == ToVector2i(src)) {
		ray.is_hit = true;
		ray.hit_point = dest;
	}
	return ray;
}

void SingleRayCasterService::SubscribePlayerPose(const vector2d& pose) {
	dest = pose;
}

void SingleRayCasterService::PrepareRay(const vector2d& src, Ray& ray,
										vector2d& ray_unit_step,
										vector2d& ray_length_1d, vector2i& step,
										vector2i& map_check) {
	ray.origin = src;
	ray.direction = dest - src;
	ray.direction.Norm();
	ray.is_hit = false;
	ray.theta = std::atan2(ray.direction.y, ray.direction.x);

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
