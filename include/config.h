/**
 * @file config.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Config class for delta time
 * @version 0.1
 * @date 2024-03-14
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "types.h"

namespace wolfenstein
{

struct PlayerConfig
{
  PlayerConfig(Pose2D pose, double player_speed, double player_rot_speed)
      : initial_pose_(pose), player_speed_(player_speed),
        player_rot_speed_(player_rot_speed)
  {
  }
  Pose2D initial_pose_;
  double player_speed_;
  double player_rot_speed_;
};

struct GeneralConfig
{

  GeneralConfig(ui16 width, ui16 height, ui16 padding, ui16 scale, double fov,
                ui16 fps)
      : width_(width), height_(height), padding_(padding), scale_(scale),
        fov_(fov), fps_(fps)
  {
    half_width_ = width / 2;
    half_height_ = height / 2;
  }
  ui16 width_;
  ui16 height_;
  ui16 padding_;
  ui16 scale_;
  double fov_;
  ui16 fps_;
  ui16 half_width_;
  ui16 half_height_;
};

} // namespace wolfenstein

#endif
