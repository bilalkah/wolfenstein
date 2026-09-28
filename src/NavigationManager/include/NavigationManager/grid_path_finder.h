/**
 * @file grid_path_finder.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Allocation-free weighted A* on a 4-connected grid
 * @version 0.1
 * @date 2026-09-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_GRID_PATH_FINDER_H
#define NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_GRID_PATH_FINDER_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace wolfenstein {

struct GridCell
{
	int x = 0;	// row
	int y = 0;	// column
	friend bool operator==(const GridCell&, const GridCell&) = default;
};

// Weighted A* on a 4-connected grid with a Euclidean heuristic. A step costs
// 1, plus whatever extra the cell it enters is given; f = (1 - w) * g + w * h,
// so a weight above 0.5 favours the heuristic (fewer expansions, not always
// the shortest path).
//
// Queries do not allocate once the grid is set: every per-cell array is sized
// by SetGrid and reused, and a generation counter marks which entries belong
// to the current query, so nothing is cleared between queries. The arrays
// come from the memory resource it is given (a level's arena, in the game).
class GridPathFinder
{
  public:
	explicit GridPathFinder(
		double heuristic_weight,
		std::pmr::memory_resource* memory = std::pmr::get_default_resource());

	// Bytes one SetGrid of this size takes from the memory resource,
	// alignment padding included: what an arena must budget for it
	static std::size_t MemoryFor(int height, int width);

	// blocked is row-major (x * width + y); non-zero cells are walls.
	// Allocates only when the grid grows.
	void SetGrid(int height, int width, std::span<const std::uint8_t> blocked);
	// The same, asking is_wall(x, y) for each cell: callers deriving the
	// grid from something else (a map) need no intermediate copy of it
	template <std::predicate<int, int> IsWall>
	void SetGrid(int height, int width, IsWall&& is_wall) {
		Resize(height, width);
		for (int x = 0; x < height; ++x) {
			for (int y = 0; y < width; ++y) {
				walls_[static_cast<std::size_t>(Index({x, y}))] =
					is_wall(x, y) ? 1 : 0;
			}
		}
	}
	// Makes a step into `cell` cost 1 + `extra`: a way to take only when
	// the others are much longer. SetGrid resets every cell to no extra.
	void SetExtraCost(GridCell cell, std::uint8_t extra);
	// Cells that are not walls: no path is longer than this
	std::size_t FreeCells() const;

	// Writes the path from start to goal, both included, into `path` (reusing
	// its capacity) and returns true, or returns false with `path` empty if
	// the goal cannot be reached. Cells in `extra_blocked` are walls for this
	// query only, and a step into a cell in `crowded` costs `crowd_cost`
	// more (a way others are taking). Start and goal are always passable.
	bool FindPath(GridCell start, GridCell goal,
				  std::span<const GridCell> extra_blocked,
				  std::pmr::vector<GridCell>& path,
				  std::span<const GridCell> crowded = {},
				  float crowd_cost = 0.0f);

	int Height() const { return height_; }
	int Width() const { return width_; }
	bool Contains(GridCell cell) const;

  private:
	struct OpenEntry
	{
		float f;
		std::int32_t index;
	};

	std::int32_t Index(GridCell cell) const { return cell.x * width_ + cell.y; }
	GridCell Cell(std::int32_t index) const {
		return {index / width_, index % width_};
	}
	void Resize(int height, int width);
	void NextGeneration();

	double heuristic_weight_;
	int height_ = 0;
	int width_ = 0;
	std::pmr::vector<std::uint8_t> walls_;
	std::pmr::vector<std::uint8_t> extra_cost_;
	// A cell's entry is valid for the current query only when its stamp
	// equals generation_
	std::pmr::vector<std::uint32_t> blocked_stamp_;
	std::pmr::vector<std::uint32_t> crowded_stamp_;
	std::pmr::vector<std::uint32_t> seen_stamp_;
	std::pmr::vector<std::uint32_t> closed_stamp_;
	std::pmr::vector<float> g_;
	std::pmr::vector<std::int32_t> parent_;
	std::pmr::vector<OpenEntry> open_;	// binary min-heap on f
	std::uint32_t generation_ = 0;
};

}  // namespace wolfenstein

#endif	// NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_GRID_PATH_FINDER_H
