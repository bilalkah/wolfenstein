#include "Camera/raycaster.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

namespace wolfenstein {

RayCaster::RayCaster(int num_ray, double fov, double depth)
	: depth_(depth), num_ray_(num_ray), half_width_(std::tan(fov / 2)) {}

namespace {

void PrepareRay(const Position2D& position, const double ray_theta, Ray& ray,
				vector2d& ray_unit_step, vector2d& ray_length_1d,
				vector2i& step, vector2i& map_check);

// Where the ray meets a door's plane, if that is inside the door's cell and
// on the part of the doorway the door still covers
bool HitDoor(Ray& ray, const Door& door) {
	const double plane = (door.across_x ? door.x : door.y) + 0.5;
	const double origin = door.across_x ? ray.origin.x : ray.origin.y;
	const double direction = door.across_x ? ray.direction.x : ray.direction.y;
	if (direction == 0.0) {
		return false;  // along the door, never through it
	}
	const double t = (plane - origin) / direction;
	if (t <= 0.0) {
		return false;
	}
	const vector2d point = ray.origin + ray.direction * t;
	const double along = door.across_x ? point.y - door.y : point.x - door.x;
	if (along < door.openness || along >= 1.0) {
		return false;  // through the open part
	}
	ray.is_hit = true;
	ray.distance = t;
	ray.perpendicular_distance = t;
	ray.hit_point = point;
	ray.is_hit_vertical = door.across_x;
	ray.texture_shift = door.openness;
	return true;
}

// A secret sliding back is a block between cells, not in the grid: the ray
// stops on it if it meets it before whatever the DDA found
void HitMovingPushWalls(Ray& ray, const Map& map, double depth) {
	for (const PushWall& wall : map.GetPushWalls()) {
		if (!wall.moving) {
			continue;
		}
		const std::array<double, 2> low{wall.x + wall.dx * wall.offset,
										wall.y + wall.dy * wall.offset};
		const std::array<double, 2> origin{ray.origin.x, ray.origin.y};
		const std::array<double, 2> direction{ray.direction.x, ray.direction.y};
		double enter = 0.0;
		double leave = ray.is_hit ? ray.distance : depth;
		int enter_axis = -1;
		bool missed = false;
		for (std::size_t axis = 0; axis < 2 && !missed; ++axis) {
			if (direction[axis] == 0.0) {
				missed =
					origin[axis] < low[axis] || origin[axis] > low[axis] + 1;
				continue;
			}
			double near = (low[axis] - origin[axis]) / direction[axis];
			double far = (low[axis] + 1 - origin[axis]) / direction[axis];
			if (near > far) {
				std::swap(near, far);
			}
			if (near > enter) {
				enter = near;
				enter_axis = static_cast<int>(axis);
			}
			leave = std::min(leave, far);
			missed = enter > leave;
		}
		if (missed || enter_axis < 0) {
			continue;  // no hit, or the ray starts inside it
		}
		ray.is_hit = true;
		ray.distance = enter;
		ray.perpendicular_distance = enter;
		ray.hit_point = ray.origin + ray.direction * enter;
		ray.wall_id = wall.texture;
		// A face across x shows the texture along y, and the other way round;
		// the texture moves with the block
		ray.is_hit_vertical = enter_axis == 0;
		const double along =
			ray.is_hit_vertical ? ray.hit_point.y : ray.hit_point.x;
		const double side = ray.is_hit_vertical ? low[1] : low[0];
		ray.texture_shift = std::fmod(along, 1.0) - (along - side);
	}
}

// DDA through the cells from `position` at `ray_theta`, until a wall, a
// closed part of a door or `depth`
Ray Cast(const Map& map, const Position2D& position, double ray_theta,
		 double depth) {
	const auto cells = map.GetCells();
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
			if (Map::IsDoorCell(cell)) {
				if (HitDoor(ray, map.GetDoors()[cell - Map::kDoorCell])) {
					ray.wall_id = cell;
				}
			}
			else if (cell != 0) {
				ray.is_hit = true;
				ray.hit_point = ray.origin + ray.direction * ray.distance;
				ray.wall_id = cell;
			}
		}
	}
	HitMovingPushWalls(ray, map, depth);
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
	// Ray i crosses the plane at the left edge of its pair of columns, where
	// Across places a sprite that starts there
	for (std::size_t i = 0; i < rays.size(); ++i) {
		const double across = 2.0 * static_cast<double>(i) / num_ray_ - 1.0;
		rays[i] =
			Cast(map, position,
				 position.theta + std::atan(across * half_width_), depth_);
	}
}

Ray CastRay(const Map& map, const Position2D& from, double theta,
			double depth) {
	return Cast(map, from, theta, depth);
}

double RayCaster::Across(double camera_angle) const {
	// A right angle off is at infinity on the plane, and beyond it behind the
	// eye: held just short of it, such a direction lands far off that side
	constexpr double kLimit = std::numbers::pi / 2 - 1e-3;
	return std::tan(std::clamp(camera_angle, -kLimit, kLimit)) / half_width_;
}

}  // namespace wolfenstein
