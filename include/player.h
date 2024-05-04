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
  Player(const PlayerConfig& config);
  void Movement(const double delta_time);
  void Update(const double delta_time);

  Pose2D GetPose() const;
  vector2i GetMapPose() const;
  std::vector<Ray> GetRays() const;
  void SetMap(const std::shared_ptr<Map>& map);

 private:
  void MoveIfNotCollision(double dx, double dy);
  Pose2D pose;
  double player_speed_;
  double player_rot_speed_;

  std::shared_ptr<Map> map_ptr_;
  std::shared_ptr<RayCaster> ray_caster_;
  std::vector<Ray> ray_;
};

}  // namespace wolfenstein

#endif
