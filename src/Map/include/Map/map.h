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

#ifndef MAP_INCLUDE_MAP_MAP_H_
#define MAP_INCLUDE_MAP_MAP_H_

#include "Math/vector.h"
#include "path-planning/planning/utility/common_planning.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace wolfenstein {

typedef std::vector<std::vector<uint16_t>> MapRaw;
class Map
{
  public:
	explicit Map(const std::string& map_path, double resolution = 0.5);

	const MapRaw& GetRawMap() const;
	std::shared_ptr<planning::Map> GetPathFinderMap();
	const uint16_t GetSizeX() const;
	const uint16_t GetSizeY() const;
	double GetResolution();
	const std::vector<uint16_t>& operator[](size_t i) const;

	// Whether a cell is inside the map
	bool Contains(int x, int y) const;
	// Whether a cell blocks movement and line of sight. Cells outside the map
	// count as blocked, so nothing walks or sees past its edge.
	bool IsBlocked(int x, int y) const;
	// The same for the cell containing a world position. Positions are
	// floored, not truncated: truncation would map -0.5 to cell 0.
	bool IsBlocked(const vector2d& position) const;

  private:
	void LoadMap(const std::string& map_path);
	void MapToPathFinderMap();

	uint16_t size_x_{};
	uint16_t size_y_{};
	std::shared_ptr<planning::Map> path_finder_map_;
	MapRaw map_;
	double res{0.5};
};

}  // namespace wolfenstein

#endif	// MAP_INCLUDE_MAP_MAP_H_
