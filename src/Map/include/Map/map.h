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

#include <cstdint>
#include <vector>

namespace wolfenstein {

class Map
{
  public:
	Map();
	void LoadMap();
	std::vector<std::vector<uint16_t>> GetMap();
	uint16_t GetSizeX();
	uint16_t GetSizeY();

  private:
	std::vector<std::vector<uint16_t>> map_;
	uint16_t size_x_;
	uint16_t size_y_;
};

}  // namespace wolfenstein

#endif // MAP_INCLUDE_MAP_MAP_H_
