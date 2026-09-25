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
#include "GameObjects/object_id.h"
#include "Math/vector.h"
#include "NavigationManager/grid_path_finder.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
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
	// Plans a path for the enemy with the given id and returns the point it
	// should head for next
	vector2d FindPath(Position2D start, Position2D end, ObjectId id);
	vector2d FindPathToPlayer(Position2D start, ObjectId id);
	// The first cells of the enemy's current path (for the 2D view)
	std::span<const GridCell> GetPath(ObjectId id) const;
	void ResetPath(ObjectId id);
	// World position of a cell's centre
	static vector2d CellCentre(GridCell cell);
	void SetPositionPtr(const std::shared_ptr<Position2D>& position);
	double EuclideanDistanceToPlayer(const Position2D& position);
	double ManhattanDistanceToPlayer(const Position2D& position);

  private:
	NavigationManager() = default;

	static GridCell ToCell(const vector2d& position);
	// Other enemies and the cells they are about to enter block the path
	void CollectDynamicObstacles();

	static NavigationManager* instance_;
	std::shared_ptr<Scene> scene_;
	// Weight 0.6: f = 0.4 g + 0.6 h, the tuning the game has always used
	GridPathFinder path_finder_{0.6};
	// The start of an enemy's current path, stored inline. Only the next
	// cell or two steer the enemy (and its next cell blocks the others); the
	// rest is for the 2D view. Reserving room for a path through every free
	// cell instead took 17 KB per enemy.
	struct Route
	{
		static constexpr std::size_t kCapacity = 64;
		std::array<GridCell, kCapacity> cells{};
		std::uint32_t size = 0;
	};
	// Indexed by ObjectId, sized once per level
	std::vector<Route> routes_;
	// Scratch buffers reused by every query
	std::vector<GridCell> obstacles_;
	std::vector<GridCell> cells_;
	std::shared_ptr<Position2D> player_position_ptr_;
};

}  // namespace wolfenstein

#endif	// NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_MANAGER_H