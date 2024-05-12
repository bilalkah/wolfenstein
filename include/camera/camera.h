/**
 * @file camera.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Camera class to apply ray casting and calculate the view
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_CAMERA_H
#define CAMERA_CAMERA_H

#include "camera/ray_cast.h"
#include "character/i_character.h"
#include "map/map.h"
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
	Camera2D();
	Camera2D(const Camera2DConfig& config);

	void Update(const Position2D& position);
	void SetMap(const std::shared_ptr<Map>& map);
	std::shared_ptr<Map> GetMap() const;
	std::shared_ptr<RayVector> GetRays() const;
	std::shared_ptr<Ray> GetCrosshairRay() const;

  private:
	void InitRays();
	Camera2DConfig config_;
	std::shared_ptr<RayVector> rays_;
	std::shared_ptr<RayCast> ray_cast_;
	std::shared_ptr<Ray> crosshair_ray_;
	std::shared_ptr<Map> map_ptr_;
};

}  // namespace wolfenstein

#endif	// CAMERA_CAMERA_H