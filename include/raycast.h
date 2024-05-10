

#ifndef RAYCAST_H_
#define RAYCAST_H_

#include "map.h"
#include "types.h"

#include <memory>
#include <vector>

namespace wolfenstein {

class RayCaster {
 public:
  RayCaster(const uint16_t number_of_rays, const double ray_length,
            const double fov);
  void SetMap(const std::shared_ptr<Map> map);
  std::vector<Ray> Cast(const Position2D position);

 private:
  uint16_t number_of_rays_;
  double ray_length_;
  double fov_;
  std::vector<double> pre_calculated_angles;
  std::shared_ptr<Map> map_ptr_;
};

}  // namespace wolfenstein
#endif
