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
#include "Math/vector.h"
#include <cmath>

namespace wolfenstein {

Camera2D::Camera2D(const Camera2DConfig& config)
	: config_(config),
	  ray_cast_(std::make_shared<RayCaster>(config.width / 2, config.fov,
											config.depth)) {
	InitRays();
}

void Camera2D::Update(const std::shared_ptr<Map>& map_ptr) {
	ray_cast_->Update(map_ptr, position_, rays_);
	crosshair_ray_ = std::make_shared<Ray>(rays_->at(config_.width / 4));
}

std::shared_ptr<RayVector> Camera2D::GetRays() const {
	return rays_;
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

Ray Camera2D::CalculateObjectRay(
	const std::shared_ptr<IGameObject>& object) const {
	Ray object_ray{};
	auto static_object = std::dynamic_pointer_cast<StaticObject>(object);
	const auto object_pose = static_object->GetPose();
	const auto w = static_object->GetWidth();

	// check if object is in the camera view
	auto object_distance = Distance(object_pose, position_.pose);
	if (object_distance > config_.depth) {
		return object_ray;
	}

	const auto object_center_angle = std::atan2(
		object_pose.y - position_.pose.y, object_pose.x - position_.pose.x);

	auto object_left_edge_angle =
		SubRadian(object_center_angle, ToRadians(90.0));
	const auto left_edge_point =
		object_pose + vector2d{w * std::cos(object_left_edge_angle),
							   w * std::sin(object_left_edge_angle)};
	const auto left_edge_angle =
		std::atan2(left_edge_point.y - position_.pose.y,
				   left_edge_point.x - position_.pose.x);
	object_ray.theta =
		left_edge_angle > 0 ? left_edge_angle : 2 * M_PI + left_edge_angle;
	object_ray.is_hit = true;
	object_ray.perpendicular_distance = object_distance;
	object_ray.wall_id = static_object->GetTextureId();

	return object_ray;
}

void Camera2D::InitRays() {
	rays_ = std::make_shared<RayVector>();
	for (int i = 0; i < config_.width / 2; i++) {
		rays_->emplace_back(Ray());
	}
}

}  // namespace wolfenstein