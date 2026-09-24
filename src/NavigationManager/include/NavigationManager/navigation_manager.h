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

#include "Characters/enemy.h"
#include "Math/vector.h"
#include "NavigationManager/grid_path_finder.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace wolfenstein {

class Scene;

class NavigationManager
{
  public:
	// Side of a pathfinding cell in world units: each map cell is split into
	// (1 / kCellSize)^2 cells, so enemies can pass each other and route
	// around one another inside a corridor
	static constexpr double kCellSize = 0.5;

	static NavigationManager& GetInstance();
	NavigationManager(const NavigationManager&) = delete;
	NavigationManager& operator=(const NavigationManager&) = delete;
	~NavigationManager();

	// Builds the pathfinding grid for the scene's map (once per level)
	void InitManager(const std::shared_ptr<Scene>& scene);
	vector2d FindPath(Position2D start, Position2D end, const std::string& id);
	vector2d FindPathToPlayer(Position2D start, const std::string& id);
	const std::vector<vector2d>& GetPath(const std::string& id);
	void ResetPath(const std::string& id);
	void SetPositionPtr(const std::shared_ptr<Position2D>& position);
	double EuclideanDistanceToPlayer(const Position2D& position);
	double ManhattanDistanceToPlayer(const Position2D& position);

  private:
	NavigationManager() = default;

	static GridCell ToCell(const vector2d& position);
	static vector2d CellCentre(GridCell cell);
	// Other enemies and the cells they are about to enter block the path
	void CollectDynamicObstacles();

	static NavigationManager* instance_;
	std::shared_ptr<Scene> scene_;
	// Weight 0.6: f = 0.4 g + 0.6 h, the tuning the game has always used
	GridPathFinder path_finder_{0.6};
	// Waypoints per enemy; the vectors keep their capacity between queries
	std::unordered_map<std::string, std::vector<vector2d>> paths_;
	// Scratch buffers reused by every query
	std::vector<GridCell> obstacles_;
	std::vector<GridCell> cells_;
	std::vector<std::uint8_t> walls_;
	std::shared_ptr<Position2D> player_position_ptr_;
};

}  // namespace wolfenstein

#endif	// NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_MANAGER_H