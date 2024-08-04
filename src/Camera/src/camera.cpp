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

namespace wolfenstein {

Camera2D::Camera2D(const Camera2DConfig& config)
	: config_(config),
	  ray_cast_(
		  std::make_shared<RayCaster>(config.width, config.fov, config.depth)) {
	InitRays();
}

void Camera2D::Update(const std::shared_ptr<Map>& map_ptr) {
	ray_cast_->Update(map_ptr, position_, rays_);
	crosshair_ray_ = std::make_shared<Ray>(rays_->at(config_.width / 2));
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

void Camera2D::SetPosition(const Position2D& position) {
	position_ = position;
}

void Camera2D::InitRays() {
	rays_ = std::make_shared<RayVector>();
	for (int i = 0; i < config_.width; i++) {
		rays_->emplace_back(Ray());
	}
}

}  // namespace wolfenstein