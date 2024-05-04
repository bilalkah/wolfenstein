#include "include/raycast.h"
#include <algorithm>
#include <cmath>
#include "macros.h"

namespace wolfenstein {

RayCaster::RayCaster(const uint16_t number_of_rays, const double ray_length,
                     const double fov)
    : number_of_rays_(number_of_rays), ray_length_(ray_length), fov_(fov) {}

void RayCaster::SetMap(const std::shared_ptr<Map> map) {
  map_ptr_ = map;
}

std::vector<Ray> RayCaster::Cast(const Pose2D pose) {
  double delta_angle = fov_ / number_of_rays_;
  double ray_angle = pose.theta_ - (fov_ / 2);
  const auto& map_data = map_ptr_->GetMap();
  const auto& row_size = map_ptr_->GetSizeX();
  const auto& col_size = map_ptr_->GetSizeY();

  // DDA algorithm
  std::vector<Ray> rays{};
  for (uint16_t i = 0; i < number_of_rays_; i++) {
    vector2d ray_orig{pose.x_, pose.y_};
    vector2d ray_dir{std::cos(ray_angle), std::sin(ray_angle)};
    ray_dir.Norm();

    vector2d ray_unit_step;
    ray_unit_step.x = ray_dir.x == 0 ? 1e30 : std::abs(1 / ray_dir.x);
    ray_unit_step.y = ray_dir.y == 0 ? 1e30 : std::abs(1 / ray_dir.y);
    vector2i map_check{static_cast<int>(ray_orig.x),
                       static_cast<int>(ray_orig.y)};
    vector2d ray_length_1d;
    vector2i step;

    if (ray_dir.x < 0) {
      step.x = -1;
      ray_length_1d.x = (ray_orig.x - float(map_check.x)) * ray_unit_step.x;
    } else {
      step.x = 1;
      ray_length_1d.x = (float(map_check.x + 1) - ray_orig.x) * ray_unit_step.x;
    }

    if (ray_dir.y < 0) {
      step.y = -1;
      ray_length_1d.y = (ray_orig.y - float(map_check.y)) * ray_unit_step.y;
    } else {
      step.y = 1;
      ray_length_1d.y = (float(map_check.y + 1) - ray_orig.y) * ray_unit_step.y;
    }

    bool ray_hit = false;
    bool is_x_side;
    float ray_distance = 0.0f;
    while (!ray_hit && ray_distance < ray_length_) {
      if (ray_length_1d.x < ray_length_1d.y) {
        map_check.x += step.x;
        ray_distance = ray_length_1d.x;
        ray_length_1d.x += ray_unit_step.x;
        is_x_side = true;
      } else {
        is_x_side = false;
        map_check.y += step.y;
        ray_distance = ray_length_1d.y;
        ray_length_1d.y += ray_unit_step.y;
      }

      if (map_check.x >= 0 && map_check.x < row_size && map_check.y >= 0 &&
          map_check.y < col_size) {
        if (map_data[map_check.x][map_check.y] != 0) {
          ray_hit = true;
        }
      }
    }
    Ray intersected_ray(ray_dir);
    if (ray_hit) {
      intersected_ray.length_ = ray_distance;
      if (is_x_side == true) {
        intersected_ray.perp_wall_dist_ = (ray_length_1d.x - ray_unit_step.x);
      } else {
        intersected_ray.perp_wall_dist_ = (ray_length_1d.y - ray_unit_step.y);
      }
      intersected_ray.is_x_side_ = is_x_side;
      intersected_ray.wall_color_ = map_data[map_check.x][map_check.y];
    }
    intersected_ray.angle_ = ray_angle;
    rays.push_back(intersected_ray);
    ray_angle += delta_angle;
  }

  return rays;
}

}  // namespace wolfenstein
