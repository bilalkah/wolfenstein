/**
 * @file map/map.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-12
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef GAME_MAP_INCLUDE_GAME_MAP_MAP_H_
#define GAME_MAP_INCLUDE_GAME_MAP_MAP_H_

#include "Math/vector.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <mdspan>
#include <string>
#include <vector>

namespace wolfenstein {

// The level's grid of cells: 0 is free, anything else a wall whose value is
// its texture id. Cells are stored in one row-major block (a vector of rows
// would allocate once per row and scatter them across the heap) and read
// through std::mdspan, so cell (x, y) is GetCells()[x, y].
class Map
{
  public:
	using CellView =
		std::mdspan<const std::uint16_t, std::dextents<std::size_t, 2>>;

	// Reads a map file, or describes what is wrong with it
	static std::expected<Map, std::string> FromFile(const std::string& path);
	// Reads a map file and exits if it cannot: the level cannot run without
	explicit Map(const std::string& map_path);

	CellView GetCells() const {
		return CellView(cells_.data(), size_x_, size_y_);
	}
	std::uint16_t GetSizeX() const { return size_x_; }
	std::uint16_t GetSizeY() const { return size_y_; }
	// Whether a cell is inside the map
	bool Contains(int x, int y) const;
	// Whether a cell blocks movement and line of sight. Cells outside the map
	// count as blocked, so nothing walks or sees past its edge.
	bool IsBlocked(int x, int y) const;
	// The same for the cell containing a world position. Positions are
	// floored, not truncated: truncation would map -0.5 to cell 0.
	bool IsBlocked(const vector2d& position) const;

  private:
	Map() = default;

	std::uint16_t size_x_{};  // rows
	std::uint16_t size_y_{};  // columns
	std::vector<std::uint16_t> cells_;
};

}  // namespace wolfenstein

#endif	// GAME_MAP_INCLUDE_GAME_MAP_MAP_H_
