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

#include <cstdint>
#include <span>
#include <vector>

namespace wolfenstein {

struct GridCell
{
	int x = 0;	// row
	int y = 0;	// column
	friend bool operator==(const GridCell&, const GridCell&) = default;
};

// Weighted A* on a 4-connected grid with unit step cost and a Euclidean
// heuristic; f = (1 - w) * g + w * h, so a weight above 0.5 favours the
// heuristic (fewer expansions, not always the shortest path).
//
// Queries do not allocate once the grid is set: every per-cell array is sized
// by SetGrid and reused, and a generation counter marks which entries belong
// to the current query, so nothing is cleared between queries.
class GridPathFinder
{
  public:
	explicit GridPathFinder(double heuristic_weight);

	// blocked is row-major (x * width + y); non-zero cells are walls.
	// Allocates only when the grid grows.
	void SetGrid(int height, int width, std::span<const std::uint8_t> blocked);

	// Writes the path from start to goal, both included, into `path` (reusing
	// its capacity) and returns true, or returns false with `path` empty if
	// the goal cannot be reached. Cells in `extra_blocked` are walls for this
	// query only. Start and goal are always passable.
	bool FindPath(GridCell start, GridCell goal,
				  std::span<const GridCell> extra_blocked,
				  std::vector<GridCell>& path);

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
	void NextGeneration();

	double heuristic_weight_;
	int height_ = 0;
	int width_ = 0;
	std::vector<std::uint8_t> walls_;
	// A cell's entry is valid for the current query only when its stamp
	// equals generation_
	std::vector<std::uint32_t> blocked_stamp_;
	std::vector<std::uint32_t> seen_stamp_;
	std::vector<std::uint32_t> closed_stamp_;
	std::vector<float> g_;
	std::vector<std::int32_t> parent_;
	std::vector<OpenEntry> open_;  // binary min-heap on f
	std::uint32_t generation_ = 0;
};

}  // namespace wolfenstein

#endif	// NAVIGATION_MANAGER_INCLUDE_NAVIGATION_MANAGER_GRID_PATH_FINDER_H
