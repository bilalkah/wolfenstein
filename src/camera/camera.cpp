#include "camera/camera.h"
#include "camera/ray_cast.h"

namespace wolfenstein {

Camera2D::Camera2D() {}

Camera2D::Camera2D(const Camera2DConfig& config)
	: config_(config),
	  ray_cast_(
		  std::make_shared<RayCast>(config.width, config.fov, config.depth)) {
	InitRays();
}

void Camera2D::Update(const Position2D& position) {
	ray_cast_->Update(position, rays_);
	crosshair_ray_ = std::make_shared<Ray>(rays_->at(config_.width / 2));
}

void Camera2D::SetMap(const std::shared_ptr<Map>& map) {
	map_ptr_ = map;
	ray_cast_->SetMap(map);
}

std::shared_ptr<Map> Camera2D::GetMap() const {
	return map_ptr_;
}

std::shared_ptr<RayVector> Camera2D::GetRays() const {
	return rays_;
}

std::shared_ptr<Ray> Camera2D::GetCrosshairRay() const {
	return crosshair_ray_;
}

void Camera2D::InitRays() {
	rays_ = std::make_shared<RayVector>();
	for (int i = 0; i < config_.width; i++) {
		rays_->emplace_back(Ray());
	}
}

}  // namespace wolfenstein