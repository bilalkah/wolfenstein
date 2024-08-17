/**
 * @file scene.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_SCENE_H_
#define GRAPHICS_INCLUDE_GRAPHICS_SCENE_H_

#include <memory>
#include <vector>

#include <GameObjects/game_object.h>
#include <Map/map.h>

namespace wolfenstein {

class Scene
{
  public:
	Scene() = default;

	void AddObject(std::shared_ptr<IGameObject> object);
	void SetMap(std::shared_ptr<Map> map);

	void Update(double delta_time);

	std::vector<std::shared_ptr<IGameObject>> GetObjects() const;
	std::shared_ptr<Map> GetMap() const;

  private:
	std::vector<std::shared_ptr<IGameObject>> objects;
	std::shared_ptr<Map> map;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_SCENE_H_