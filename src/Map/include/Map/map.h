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

typedef std::vector<std::vector<uint16_t>> MapRaw;
class Map
{
  public:
	Map(std::string map_path =
			"/home/bikahraman/wolfenstein/assets/maps/map1.txt",
		double resolution = 0.5);

	const MapRaw& GetRawMap() const;
	std::shared_ptr<planning::Map> GetPathFinderMap();
	const uint16_t GetSizeX() const;
	const uint16_t GetSizeY() const;
	double GetResolution();
	const std::vector<uint16_t>& operator[](size_t i) const;

  private:
	void LoadMap(std::string map_path);
	void MapToPathFinderMap();

	uint16_t size_x_;
	uint16_t size_y_;
	std::shared_ptr<planning::Map> path_finder_map_;
	MapRaw map_;
	double res{0.5};
};

}  // namespace wolfenstein

#endif	// MAP_INCLUDE_MAP_MAP_H_
