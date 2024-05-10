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

namespace wolfenstein {

struct PlayerConfig {
  PlayerConfig(Position2D position, double player_speed,
               double player_rot_speed)
      : position_(position),
        player_speed_(player_speed),
        player_rot_speed_(player_rot_speed) {}
  Position2D position_;
  double player_speed_;
  double player_rot_speed_;
};

struct GeneralConfig {

  GeneralConfig(uint16_t width, uint16_t height, uint16_t padding,
                uint16_t scale, uint16_t fps, double view_distance, double fov)
      : width_(width),
        height_(height),
        padding_(padding),
        scale_(scale),
        fps_(fps),
        view_distance_(view_distance),
        fov_(fov) {
    half_width_ = width / 2;
    half_height_ = height / 2;
  }
  uint16_t width_;
  uint16_t height_;
  uint16_t padding_;
  uint16_t scale_;
  uint16_t half_width_;
  uint16_t half_height_;
  uint16_t fps_;
  double view_distance_;
  double fov_;
};

}  // namespace wolfenstein

#endif
