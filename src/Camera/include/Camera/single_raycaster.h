/**
 * @file single_raycaster.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-11-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CAMERA_INCLUDE_CAMERA_SINGLE_RAYCASTER_H_
#define CAMERA_INCLUDE_CAMERA_SINGLE_RAYCASTER_H_

#include "Camera/ray.h"
#include "Characters/character.h"
#include "Map/map.h"
#include "Math/vector.h"

#include <memory>

namespace wolfenstein {

class Scene;

class SingleRayCasterService
{
  public:
	static SingleRayCasterService& GetInstance();
	SingleRayCasterService(const SingleRayCasterService&) = delete;
	SingleRayCasterService& operator=(const SingleRayCasterService&) = delete;
	~SingleRayCasterService();

	void InitService(const std::shared_ptr<Scene>& scene_ptr);
	Ray Cast(const vector2d& src);

	void SetDestinationPtr(const std::shared_ptr<Position2D>& position_ptr);

  private:
	SingleRayCasterService() = default;

	void PrepareRay(const vector2d& src, Ray& ray, vector2d& ray_unit_step,
					vector2d& ray_length_1d, vector2i& step,
					vector2i& map_check);

	std::shared_ptr<Position2D> dest_ptr_;
	std::shared_ptr<Scene> scene_ptr_;
	static SingleRayCasterService* instance_;
};

}  // namespace wolfenstein

#endif	// CAMERA_INCLUDE_CAMERA_SINGLE_RAYCASTER_H_
