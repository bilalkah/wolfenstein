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

#ifndef CAMERA_INCLUDE_CAMERA_H
#define CAMERA_INCLUDE_CAMERA_H

#include "Camera/ray.h"
#include "Camera/raycaster.h"
#include "Characters/character.h"
#include "Core/scene.h"
#include "GameObjects/game_object.h"
#include "GameObjects/static_object.h"
#include "Map/map.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace wolfenstein {

struct Camera2DConfig
{
	int width;
	double fov;
	double depth;
};

typedef std::pair<Ray, Ray> RayPair;

class Camera2D
{
  public:
	explicit Camera2D(const Camera2DConfig& config);
	~Camera2D() = default;

	void Update(const std::shared_ptr<Scene>& scene);

	std::shared_ptr<RayVector> GetRays() const;
	std::shared_ptr<Ray> GetCrosshairRay() const;
	std::optional<RayPair> GetObjectRay(const std::string id);
	Position2D GetPosition() const;
	double GetFov() const;
	double GetDeltaAngle() const;

	void SetPosition(const Position2D& position);

  private:
	void InitRays();
	void Calculate(const std::shared_ptr<IGameObject>& object);
	double WorldAngleToCameraAngle(double angle) const;

	Camera2DConfig config_;
	Position2D position_;
	std::shared_ptr<RayCaster> ray_cast_;
	std::shared_ptr<RayVector> rays_;
	std::shared_ptr<Ray> crosshair_ray_;
	std::unordered_map<std::string, RayPair> objects_;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_H