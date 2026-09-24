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
#include "GameObjects/game_object.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>

namespace wolfenstein {

struct Camera2DConfig
{
	Camera2DConfig(int width, double fov, double depth)
		: width(width), fov(fov), depth(depth) {}

	int width;
	double fov;
	double depth;
};

typedef std::pair<Ray, Ray> RayPair;

class Scene;
class Camera2D
{
  public:
	explicit Camera2D(const Camera2DConfig& config,
					  const std::shared_ptr<Scene> scene = nullptr);

	void Update();

	void SetScene(const std::shared_ptr<Scene>& scene);
	const RayVector& GetRays() const;
	const std::shared_ptr<Ray>& GetCrosshairRay() const;
	// The rays bounding an object in the current frame, or nullptr if the
	// object is not visible
	const RayPair* FindObjectRays(const std::string& id) const;
	Position2D GetPosition() const;
	double GetFov() const;
	double GetDeltaAngle() const;

	void SetPositionPtr(const std::shared_ptr<Position2D> position);

  private:
	void InitRays();
	void Calculate(const IGameObject& object);
	double WorldAngleToCameraAngle(double angle) const;

	Camera2DConfig config_;
	std::shared_ptr<Scene> scene_;
	std::shared_ptr<Position2D> position_;
	std::shared_ptr<Ray> crosshair_ray_;
	std::unique_ptr<RayCaster> ray_cast_;
	std::unique_ptr<RayVector> rays_;
	// Rays of each object, kept across frames so the hash map nodes are
	// allocated once per level (in SetScene) rather than every frame; an
	// entry is current only if it was written in this frame
	struct ObjectView
	{
		RayPair rays;
		std::uint64_t frame = std::numeric_limits<std::uint64_t>::max();
	};
	std::unordered_map<std::string, ObjectView> objects_;
	std::uint64_t frame_ = 0;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_H