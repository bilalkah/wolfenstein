/**
 * @file map.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-12
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef MAP_H
#define MAP_H

#include "types.h"

#include <vector>

namespace wolfenstein {

class Map {
 public:
  void LoadMap();
  std::vector<std::vector<uint16_t>> GetMap();
  uint16_t GetSizeX();
  uint16_t GetSizeY();

 private:
  std::vector<std::vector<uint16_t>> map;
  uint16_t size_x;
  uint16_t size_y;
};

}  // namespace wolfenstein

#endif
