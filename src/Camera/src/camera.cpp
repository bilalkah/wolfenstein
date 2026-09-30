/**
 * @file camera.cpp
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-21
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "Camera/camera.h"
#include "Camera/ray.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "GameObjects/game_object.h"
#include "Math/vector.h"

#include <cmath>
#include <cstddef>
#include <memory>

namespace karakale {

void Camera2D::InitRays() {
	rays_.assign(static_cast<std::size_t>(config_.width / 2), Ray());
}

Camera2D::Camera2D(const Camera2DConfig& config)
	: config_(config), ray_cast_(config.width / 2, config.fov, config.depth) {
	InitRays();
}

void Camera2D::Update(const Position2D& eye, double alpha) {
	eye_ = eye;
	ray_cast_.Update(scene_->GetMap(), eye_, rays_);
	crosshair_ray_ = rays_.at(static_cast<std::size_t>(config_.width / 4));
	crosshair_ray_.is_hit = false;

	// Update object rays; entries not written this frame count as invisible
	++frame_;
	for (const auto& object : scene_->GetObjects()) {
		Calculate(*object, alpha);
	}
}

void Camera2D::SetScene(Scene& scene) {
	scene_ = &scene;
	views_.assign(scene_->GetObjects().size(), ObjectView{});
}

void Camera2D::ExploreView() {
	const auto cell = [](double coordinate) {
		return static_cast<int>(std::floor(coordinate));
	};
	const vector2d eye = eye_.pose;
	for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
			scene_->Explore(cell(eye.x) + dx, cell(eye.y) + dy);
		}
	}
	// Every few rays is enough: neighbouring rays cross the same cells
	constexpr std::size_t kRayStride = 4;
	constexpr double kStep = 0.3;
	for (std::size_t i = 0; i < rays_.size(); i += kRayStride) {
		const Ray& ray = rays_[i];
		for (double t = 0.0; t < ray.distance; t += kStep) {
			const vector2d point = ray.origin + ray.direction * t;
			scene_->Explore(cell(point.x), cell(point.y));
		}
		if (ray.is_hit) {
			// Just past the hit point: inside the wall
			const vector2d wall = ray.hit_point + ray.direction * 1e-3;
			scene_->Explore(cell(wall.x), cell(wall.y));
		}
	}
}

const RayVector& Camera2D::GetRays() const {
	return rays_;
}

const Camera2D::Sight* Camera2D::FindObject(ObjectId id) const {
	const auto index = ToIndex(id);
	if (index >= views_.size() || views_[index].frame != frame_) {
		return nullptr;
	}
	return &views_[index].sight;
}

double Camera2D::GetFov() const {
	return config_.fov;
}
void Camera2D::SetFov(double fov) {
	config_.fov = fov;
	ray_cast_ = RayCaster(config_.width / 2, fov, config_.depth);
}
double Camera2D::Across(double camera_angle) const {
	return ray_cast_.Across(camera_angle);
}

void Camera2D::Calculate(const IGameObject& object, double alpha) {
	if (!object.IsVisible()) {
		return;
	}

	const auto object_pose = object.GetRenderPose(alpha);
	const auto& position = eye_;
	// As the eye sees it: an enemy from its side, its back
	const IGameObject::Appearance seen = object.SeenFrom(position.pose);
	const auto width = seen.width;
	// check if object is in the camera view
	auto object_distance = object_pose.Distance(position.pose);
	if (object_distance > config_.depth) {
		return;
	}
	// The eye standing in it (a lamp does not block the way): drawn, it would
	// cover the screen
	constexpr double kNearest = 0.25;
	if (object_distance < kNearest) {
		return;
	}

	// Object center angle
	const auto object_center_angle = std::atan2(
		object_pose.y - position.pose.y, object_pose.x - position.pose.x);
	// How far in front of the eye it stands, along the view, taken at its
	// centre. Its edges would not do: up close they are far off to the
	// sides, so a close enemy's seemed nearer than it was, and it was drawn
	// too tall and then, a step closer, not at all.
	const double distance =
		object_distance *
		std::cos(WorldAngleToCameraAngle(object_center_angle));
	if (distance <= 0.0) {
		return;	 // level with the eye or behind it: seen edge on
	}

	// Object left edge point and angle
	auto object_left_edge_angle =
		SubRadian(object_center_angle, ToRadians(90.0));
	const auto left_edge_point =
		object_pose + vector2d{width / 2 * std::cos(object_left_edge_angle),
							   width / 2 * std::sin(object_left_edge_angle)};
	const auto left_edge_angle =
		std::atan2(left_edge_point.y - position.pose.y,
				   left_edge_point.x - position.pose.x);
	const auto camera_angle_left = WorldAngleToCameraAngle(left_edge_angle);

	// Object right edge point and angle
	auto object_right_edge_angle =
		SumRadian(object_center_angle, ToRadians(90.0));
	const auto right_edge_point =
		object_pose + vector2d{width / 2 * std::cos(object_right_edge_angle),
							   width / 2 * std::sin(object_right_edge_angle)};
	const auto right_edge_angle =
		std::atan2(right_edge_point.y - position.pose.y,
				   right_edge_point.x - position.pose.x);
	const auto camera_angle_right = WorldAngleToCameraAngle(right_edge_angle);

	// Check if object is in the camera view
	if (camera_angle_right < -config_.fov / 2 ||
		camera_angle_left > config_.fov / 2) {
		return;
	}
	const auto texture_id = seen.texture_id;

	// Calculate object raypair
	RayPair object_ray_pair;
	object_ray_pair.first.Reset(position.pose, camera_angle_left);
	object_ray_pair.first.is_hit = true;
	object_ray_pair.first.wall_id = texture_id;

	object_ray_pair.second.Reset(position.pose, camera_angle_right);
	object_ray_pair.second.is_hit = true;
	object_ray_pair.second.wall_id = texture_id;

	auto& view = views_[ToIndex(object.GetId())];
	view.sight = {.rays = object_ray_pair,
				  .distance = distance,
				  .mirrored = seen.mirrored,
				  .tint = seen.tint};
	view.frame = frame_;

	// Calculate if the object is in the crosshair
	if (object.GetObjectType() == ObjectType::CHARACTER_ENEMY) {
		const auto& bot = dynamic_cast<const Enemy&>(object);
		if (object_distance < crosshair_ray_.perpendicular_distance &&
			camera_angle_left <= 0 && camera_angle_right >= 0 &&
			bot.IsAlive()) {
			crosshair_ray_.is_hit = true;
			crosshair_ray_.perpendicular_distance = distance;
			crosshair_ray_.distance = object_distance;
			crosshair_ray_.object_id = bot.GetId();
			crosshair_ray_.hit_point = object_pose;
		}
	}
}

double Camera2D::WorldAngleToCameraAngle(double angle) const {
	const auto vector_of_crosshair = crosshair_ray_.direction;
	const auto vector_of_ray = vector2d{std::cos(angle), std::sin(angle)};
	const auto angle_between = CalculateAngleBetweenTwoVectorsSigned(
		vector_of_ray, vector_of_crosshair);
	return angle_between;
}

}  // namespace karakale