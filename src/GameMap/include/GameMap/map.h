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
#include <memory_resource>
#include <span>
#include <string>
#include <vector>

namespace wolfenstein {

// A sliding door in a cell between two walls facing each other. It is a
// plane across the middle of the cell, at right angles to the way through,
// that slides into the wall as it opens.
struct Door
{
	std::uint16_t x{};
	std::uint16_t y{};
	// The way through runs along x (walls at y - 1 and y + 1), so the door
	// is the plane x + 0.5; otherwise it runs along y and the door is the
	// plane y + 0.5
	bool across_x{};
	// 0 closed, 1 open: how much of the doorway is clear
	double openness{};
};

// The level's grid of cells: 0 is free, a door cell is kDoorCell plus the
// door's index, anything else a wall whose value is its texture id. Cells
// are stored in one row-major block (a vector of rows would allocate once
// per row and scatter them across the heap) and read through std::mdspan,
// so cell (x, y) is GetCells()[x, y].
class Map
{
  public:
	using CellView =
		std::mdspan<const std::uint16_t, std::dextents<std::size_t, 2>>;

	// Door cells are numbered from here; wall textures stay far below
	static constexpr std::uint16_t kDoorCell = 0x8000;
	// A door this far open lets characters and sight through
	static constexpr double kPassableOpenness = 0.8;
	static constexpr bool IsDoorCell(std::uint16_t cell) {
		return cell >= kDoorCell;
	}

	// Reads a map file, or describes what is wrong with it
	static std::expected<Map, std::string> FromFile(const std::string& path);
	// Reads a map file and exits if it cannot: the level cannot run without
	explicit Map(const std::string& map_path);
	// A copy of `other` whose cells live in `memory` (a level's arena)
	Map(const Map& other, std::pmr::memory_resource* memory);
	Map(const Map&) = default;
	Map(Map&&) noexcept = default;
	// Not assignable: a map's cells may live in an arena, and assigning
	// across memory resources would copy (and could throw) where a move is
	// expected not to
	Map& operator=(const Map&) = delete;
	Map& operator=(Map&&) = delete;
	~Map() = default;

	// Bytes a copy into a memory resource takes, padding included
	std::size_t MemoryBytes() const {
		return cells_.size() * sizeof(std::uint16_t) +
			   doors_.size() * sizeof(Door) + 2 * alignof(std::max_align_t);
	}

	CellView GetCells() const {
		return CellView(cells_.data(), size_x_, size_y_);
	}
	std::uint16_t GetSizeX() const { return size_x_; }
	std::uint16_t GetSizeY() const { return size_y_; }
	// Whether a cell is inside the map
	bool Contains(int x, int y) const;
	// Whether a cell blocks movement and line of sight: a wall, or a door
	// not open enough to pass. Cells outside the map count as blocked, so
	// nothing walks or sees past its edge.
	bool IsBlocked(int x, int y) const;
	// Whether a cell is a wall (or outside the map): blocked for good,
	// whatever the doors do
	bool IsWall(int x, int y) const;
	// The same for the cell containing a world position. Positions are
	// floored, not truncated: truncation would map -0.5 to cell 0.
	bool IsBlocked(const vector2d& position) const;

	std::span<const Door> GetDoors() const { return doors_; }
	// The door in cell (x, y), or nullptr
	const Door* FindDoor(int x, int y) const;
	void SetDoorOpenness(std::size_t door, double openness);

  private:
	Map() = default;

	std::uint16_t size_x_{};  // rows
	std::uint16_t size_y_{};  // columns
	std::pmr::vector<std::uint16_t> cells_;
	std::pmr::vector<Door> doors_;
};

}  // namespace wolfenstein

#endif	// GAME_MAP_INCLUDE_GAME_MAP_MAP_H_
