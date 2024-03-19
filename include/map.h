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

namespace wolfenstein
{

class Map
{
public:
  void LoadMap();
  std::vector<std::vector<ui16> > GetMap();
  ui16 GetSizeX();
  ui16 GetSizeY();

private:
  std::vector<std::vector<ui16> > map;
  ui16 size_x;
  ui16 size_y;
};

} // namespace wolfenstein

#endif
