#include "Camera/single_raycaster.h"
#include "Core/scene.h"
#include <cmath>

namespace wolfenstein {

SingleRayCasterService* SingleRayCasterService::instance_ = nullptr;

SingleRayCasterService& SingleRayCasterService::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new SingleRayCasterService();
	}
	return *instance_;
}

SingleRayCasterService::~SingleRayCasterService() {
	delete instance_;
}

void SingleRayCasterService::InitService(
	const std::shared_ptr<Scene>& scene_ptr) {
	scene_ptr_ = scene_ptr;
}

// Line of sight from src to the player: visible if the ray reaches the
// player's cell before any blocked cell. The loop always ends: cells outside
// the map count as blocked, and the ray gives up once it has travelled past
// the player's distance.
Ray SingleRayCasterService::Cast(const vector2d& src) {
	const vector2d dest = dest_ptr_->pose;
	const Map& map = scene_ptr_->GetMap();
	Ray ray;
	if (map.IsBlocked(src)) {
		ray.is_hit = false;
		return ray;
	}
	vector2d ray_unit_step, ray_length_1d;
	vector2i step, map_check;
	PrepareRay(src, ray, ray_unit_step, ray_length_1d, step, map_check);
	const auto depth = src.Distance(dest);
	const vector2i dest_cell = ToVector2i(dest);
	while (!ray.is_hit && ray.distance <= depth + 1.0) {
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

		if (map.IsBlocked(map_check.x, map_check.y)) {
			ray.is_hit = false;
			break;
		}
		if (map_check == dest_cell) {
			ray.is_hit = true;
			ray.hit_point = ray.origin + ray.direction * depth;
		}
	}
	if (dest_cell == ToVector2i(src)) {
		ray.is_hit = true;
		ray.hit_point = dest;
	}
	return ray;
}

void SingleRayCasterService::SetDestinationPtr(
	const std::shared_ptr<Position2D>& position_ptr) {
	dest_ptr_ = position_ptr;
}

void SingleRayCasterService::PrepareRay(const vector2d& src, Ray& ray,
										vector2d& ray_unit_step,
										vector2d& ray_length_1d, vector2i& step,
										vector2i& map_check) {
	const auto dest = dest_ptr_->pose;
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
