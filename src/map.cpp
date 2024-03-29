#include "include/map.h"
#include <iostream>
#include <sstream>
#include <string>

namespace wolfenstein
{

namespace
{

std::string simple_map{ "1111111111111111"
                        "1000000000000001"
                        "1003333000222001"
                        "1000004000002001"
                        "1000004000002001"
                        "1003333000000001"
                        "1000000000000001"
                        "1000400040000001"
                        "1113131113003101"
                        "1110000000003001"
                        "1000001113003101"
                        "1130000113003111"
                        "1400000000000001"
                        "3000000000000001"
                        "1000000000000001"
                        "1111111111111111" };
} // namespace

void
Map::LoadMap()
{
  ui16 size = 16;
  ui16 it = 0;
  for(ui16 i = 0; i < size; i++)
    {
      std::vector<ui16> row;
      for(ui16 j = 0; j < size; j++)
        {
          auto c = (simple_map[it]);
          if(c == '0')
            {
              row.push_back(0);
            }
          else if(c == '1')
            {
              row.push_back(1);
            }
          else if(c == '2')
            {
              row.push_back(2);
            }
          else if(c == '3')
            {
              row.push_back(3);
            }
          else if(c == '4')
            {
              row.push_back(4);
            }
          ++it;
        }
      map.push_back(row);
    }
  size_x = map.size();
  size_y = map[0].size();
}

std::vector<std::vector<ui16> >
Map::GetMap()
{
  return map;
}

ui16
Map::GetSizeX()
{
  return size_x;
}
ui16
Map::GetSizeY()
{
  return size_y;
}

} // namespace wolfenstein
