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

#include "path-planning/planning/utility/common_planning.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace wolfenstein {

class Map
{
  public:
	Map();
	std::vector<std::vector<uint16_t>> GetMap();
	std::shared_ptr<planning::Map> GetPathFinderMap();
	uint16_t GetSizeX();
	uint16_t GetSizeY();

  private:
	void LoadMap();
	void MapToPathFinderMap();

	uint16_t size_x_;
	uint16_t size_y_;
	std::shared_ptr<planning::Map> path_finder_map_;
	std::vector<std::vector<uint16_t>> map_;
};

}  // namespace wolfenstein

#endif	// MAP_INCLUDE_MAP_MAP_H_
