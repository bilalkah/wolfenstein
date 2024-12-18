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

#ifndef CORE_INCLUDE_CORE_SCENE_H_
#define CORE_INCLUDE_CORE_SCENE_H_

#include <memory>
#include <vector>

#include "Characters/enemy.h"
#include "Characters/player.h"
#include "GameObjects/game_object.h"
#include "Map/map.h"

namespace wolfenstein {

class Scene
{
  public:
	Scene() = default;

	void AddObject(std::shared_ptr<IGameObject> object);
	void SetMap(std::shared_ptr<Map> map);
	void SetPlayer(std::shared_ptr<Player>& player);
	void SetNextScene(const std::string next_scene);
	void DecreaseAliveEnemies();

	void Update(double delta_time);

	std::vector<std::shared_ptr<IGameObject>>& GetObjects();
	std::vector<std::shared_ptr<Enemy>>& GetEnemies();

	const Map& GetMap() const;
	Map& GetMap();
	const Player& GetPlayer() const;
	Player& GetPlayer();

	size_t GetNumberOfAliveEnemies() const;
	std::string GetNextScene() const;

  private:
	std::vector<std::shared_ptr<IGameObject>> objects;
	std::vector<std::shared_ptr<Enemy>> enemies;
	std::shared_ptr<Map> map;
	std::shared_ptr<Player> player;
	size_t number_of_alive_enemies;
	std::string next_scene_str;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_H_