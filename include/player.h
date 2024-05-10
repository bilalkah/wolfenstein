/**
 * @file player.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef PLAYER_H_
#define PLAYER_H_

#include <memory>
#include "config.h"
#include "map.h"
#include "raycast.h"
#include "types.h"
namespace wolfenstein {

class Player {
 public:
  Player(const PlayerConfig& config, uint16_t half_of_width,
         double view_distance, double fov);
  void Movement(const double delta_time);
  void Update(const double delta_time);

  Position2D GetPosition() const;
  vector2i GetMapPose() const;
  std::vector<Ray> GetRays() const;
  void SetMap(const std::shared_ptr<Map>& map);

 private:
  void MoveIfNotCollision(double dx, double dy);
  PlayerConfig config_;

  std::shared_ptr<Map> map_ptr_;
  std::shared_ptr<RayCaster> ray_caster_;
  std::vector<Ray> rays_;
};

}  // namespace wolfenstein

#endif
