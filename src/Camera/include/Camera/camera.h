/**
 * @file camera.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-21
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_CAMERA_H
#define CAMERA_CAMERA_H

#include "Camera/raycaster.h"
#include <Camera/ray.h>
#include <Characters/character.h>
#include <GameObjects/game_object.h>
#include <Map/map.h>
#include <memory>

namespace wolfenstein {

struct Camera2DConfig
{
	int width;
	double fov;
	double depth;
};

class Camera2D
{
  public:
	explicit Camera2D(const Camera2DConfig& config);
	~Camera2D() = default;

	void Update(const std::shared_ptr<Map>& map);

	std::shared_ptr<RayVector> GetRays() const;
	std::shared_ptr<Ray> GetCrosshairRay() const;
	Position2D GetPosition() const;
	double GetFov() const { return config_.fov; }
	double GetDeltaAngle() const { return config_.fov / config_.width; }

	void SetPosition(const Position2D& position);

  private:
	void InitRays();
	Camera2DConfig config_;
	std::shared_ptr<RayVector> rays_;
	std::shared_ptr<RayCaster> ray_cast_;
	std::shared_ptr<Ray> crosshair_ray_;
	Position2D position_;
};

}  // namespace wolfenstein

#endif	// CAMERA_CAMERA_H