/**
 * @file navigation_manager.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_MANAGER_H
#define NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_MANAGER_H

#include "Characters/character.h"
#include "Map/map.h"
#include "Math/vector.h"
#include "path-planning/planning/grid_base/astar/astar.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace wolfenstein {

class NavigationManager
{
  public:
	static NavigationManager& GetInstance();
	NavigationManager(const NavigationManager&) = delete;
	NavigationManager& operator=(const NavigationManager&) = delete;
	~NavigationManager() = default;

	void InitManager(std::shared_ptr<Map> map);
	vector2d FindPath(Position2D start, Position2D end, std::string id);
	vector2d FindPathToPlayer(Position2D start, std::string id);
	std::vector<vector2d> GetPath(std::string id);
	void ResetPath(std::string id);
	void SubscribePlayerPosition(const Position2D& position);
	double EuclideanDistanceToPlayer(const Position2D& position);
	double ManhattanDistanceToPlayer(const Position2D& position);

  private:
	NavigationManager() = default;

	static NavigationManager* instance_;
	std::shared_ptr<Map> map_;
	std::shared_ptr<planning::grid_base::AStar> path_planner_;
	std::unordered_map<std::string, std::vector<vector2d>> paths_;
	Position2D player_position_;
};

}  // namespace wolfenstein

#endif	// NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_MANAGER_H