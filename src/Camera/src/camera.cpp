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
#include "Core/scene.h"
#include "Math/vector.h"

#include <cmath>
#include <memory>
#include <string>

namespace wolfenstein {

void Camera2D::InitRays() {
	rays_ = std::make_shared<RayVector>();
	for (int i = 0; i < config_.width / 2; i++) {
		rays_->emplace_back(Ray());
	}
}

Camera2D::Camera2D(const Camera2DConfig& config,
				   const std::shared_ptr<Scene>& scene)
	: config_(config),
	  scene_(scene),
	  ray_cast_(std::make_shared<RayCaster>(config.width / 2, config.fov,
											config.depth)) {
	InitRays();
}

void Camera2D::Update() {
	ray_cast_->Update(scene_->GetMap(), position_, rays_);
	crosshair_ray_ = std::make_shared<Ray>(rays_->at(config_.width / 4));
	crosshair_ray_->is_hit = false;

	// Update object rays
	objects_.clear();
	for (const auto& object : scene_->GetObjects()) {
		Calculate(object);
	}
}

std::shared_ptr<RayVector> Camera2D::GetRays() const {
	return rays_;
}

std::optional<RayPair> Camera2D::GetObjectRay(const std::string id) {
	if (objects_.find(id) != objects_.end()) {
		return objects_[id];
	}
	return std::nullopt;
}

std::shared_ptr<Ray> Camera2D::GetCrosshairRay() const {
	return crosshair_ray_;
}

Position2D Camera2D::GetPosition() const {
	return position_;
}

double Camera2D::GetFov() const {
	return config_.fov;
}
double Camera2D::GetDeltaAngle() const {
	return ray_cast_->GetDeltaTheta();
}

void Camera2D::SetPosition(const Position2D& position) {
	position_ = position;
}

void Camera2D::Calculate(const std::shared_ptr<IGameObject>& object) {

	const auto object_pose = object->GetPose();
	const auto width = object->GetWidth();

	// check if object is in the camera view
	auto object_distance = object_pose.Distance(position_.pose);
	if (object_distance > config_.depth) {
		return;
	}

	// Object center angle
	const auto object_center_angle = std::atan2(
		object_pose.y - position_.pose.y, object_pose.x - position_.pose.x);

	// Object left edge point and angle
	auto object_left_edge_angle =
		SubRadian(object_center_angle, ToRadians(90.0));
	const auto left_edge_point =
		object_pose + vector2d{width / 2 * std::cos(object_left_edge_angle),
							   width / 2 * std::sin(object_left_edge_angle)};
	const auto left_edge_angle =
		std::atan2(left_edge_point.y - position_.pose.y,
				   left_edge_point.x - position_.pose.x);
	const auto camera_angle_left = WorldAngleToCameraAngle(left_edge_angle);

	// Object right edge point and angle
	auto object_right_edge_angle =
		SumRadian(object_center_angle, ToRadians(90.0));
	const auto right_edge_point =
		object_pose + vector2d{width / 2 * std::cos(object_right_edge_angle),
							   width / 2 * std::sin(object_right_edge_angle)};
	const auto right_edge_angle =
		std::atan2(right_edge_point.y - position_.pose.y,
				   right_edge_point.x - position_.pose.x);
	const auto camera_angle_right = WorldAngleToCameraAngle(right_edge_angle);

	// Check if object is in the camera view
	if (camera_angle_right < -config_.fov / 2 ||
		camera_angle_left > config_.fov / 2) {
		return;
	}
	const auto texture_id = object->GetTextureId();

	// Calculate object raypair
	RayPair object_ray_pair;
	object_ray_pair.first.Reset(position_.pose, camera_angle_left);
	object_ray_pair.first.is_hit = true;
	object_ray_pair.first.perpendicular_distance =
		object_distance * std::cos(camera_angle_left);
	object_ray_pair.first.wall_id = texture_id;

	object_ray_pair.second.Reset(position_.pose, camera_angle_right);
	object_ray_pair.second.is_hit = true;
	object_ray_pair.second.perpendicular_distance =
		object_distance * std::cos(camera_angle_right);
	object_ray_pair.second.wall_id = texture_id;

	objects_[object->GetId()] = object_ray_pair;

	// Calculate if the object is in the crosshair
	if (object_distance < crosshair_ray_->perpendicular_distance &&
		camera_angle_left <= 0 && camera_angle_right >= 0) {
		crosshair_ray_->is_hit = true;
		crosshair_ray_->perpendicular_distance = object_distance;
		crosshair_ray_->object_id = object->GetId();
		crosshair_ray_->hit_point = object_pose;
	}
}

double Camera2D::WorldAngleToCameraAngle(double angle) const {
	const auto vector_of_crosshair = crosshair_ray_->direction;
	const auto vector_of_ray = vector2d{std::cos(angle), std::sin(angle)};
	const auto angle_between = CalculateAngleBetweenTwoVectorsSigned(
		vector_of_ray, vector_of_crosshair);
	return angle_between;
}

}  // namespace wolfenstein