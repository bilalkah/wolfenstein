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
#include "GameObjects/object_id.h"
#include "Math/vector.h"
#include "NavigationManager/grid_path_finder.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace wolfenstein {

class Scene;

// Plans the enemies' paths through one level. The level's Scene owns it and
// it borrows the scene (the map, the enemies, the player), so it lives and
// dies with its level: nothing to re-initialise when the next one loads.
class NavigationManager
{
  public:
	// Side of a pathfinding cell in world units: each map cell is split into
	// (1 / kCellSize)^2 cells, so enemies can pass each other and route
	// around one another inside a corridor
	static constexpr double kCellSize = 0.5;

	// Takes every buffer from `memory` (the level's arena)
	NavigationManager(const Scene& scene, std::pmr::memory_resource* memory);

	// Bytes Build takes from the memory resource for a map this size with
	// this many objects and enemies: what the level's arena budgets for it
	static std::size_t MemoryFor(int map_rows, int map_cols,
								 std::size_t objects, std::size_t enemies);
	// Refers to its scene
	NavigationManager(const NavigationManager&) = delete;
	NavigationManager& operator=(const NavigationManager&) = delete;
	NavigationManager(NavigationManager&&) = delete;
	NavigationManager& operator=(NavigationManager&&) = delete;
	~NavigationManager() = default;

	// Builds the pathfinding grid from the scene's map and sizes the per-enemy
	// routes; call once the level's map, objects and enemies are in place
	void Build();
	// Plans a path for the enemy with the given id and returns the point it
	// should head for next: along the path, and round any lamp in the way
	vector2d FindPath(Position2D start, Position2D end, ObjectId id);
	vector2d FindPathToPlayer(Position2D start, ObjectId id);
	// The first cells of the enemy's current path (for the 2D view)
	std::span<const GridCell> GetPath(ObjectId id) const;
	void ResetPath(ObjectId id);
	// World position of a cell's centre
	static vector2d CellCentre(GridCell cell);
	double EuclideanDistanceToPlayer(const Position2D& position) const;

  private:
	// Something solid that stands for good (a lamp): a disc on the floor
	struct Solid
	{
		vector2d centre;
		double radius = 0.0;
	};
	// Steps into a cell a solid crowds (its centre is within the widest
	// enemy's reach of it) cost this much more: routes keep clear of lamps
	// where there is room, and squeeze past them only where there is not
	static constexpr std::uint8_t kSqueezeCost = 4;

	static GridCell ToCell(const vector2d& position);
	// Other enemies than `self` and the cells they are about to enter block
	// the path
	void CollectDynamicObstacles(ObjectId self);
	// Where a body of `radius` at `from` should head for `target`: straight
	// there, or, if a solid is in the way, round it on the target's side (a
	// body pressing straight into it would wedge there). A target within a
	// solid's reach is moved out of it first.
	vector2d SteerRound(const vector2d& from, vector2d target,
						double radius) const;

	const Scene& scene_;
	GridPathFinder path_finder_;
	std::pmr::vector<Solid> solids_;
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
	std::pmr::vector<Route> routes_;
	// Scratch buffers reused by every query
	std::pmr::vector<GridCell> obstacles_;
	std::pmr::vector<GridCell> cells_;
};

}  // namespace wolfenstein

#endif	// NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_NAVIGATION_MANAGER_H