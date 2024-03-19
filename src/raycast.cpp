#include "include/raycast.h"
#include "macros.h"
#include <algorithm>
#include <cmath>

namespace wolfenstein
{

RayCaster::RayCaster(const ui16 number_of_rays, const double ray_length,
                     const double fov)
    : number_of_rays_(number_of_rays), ray_length_(ray_length), fov_(fov)
{
}

void
RayCaster::SetMap(const std::shared_ptr<Map> map)
{
  map_ptr_ = map;
}

std::vector<Ray>
RayCaster::Cast(const Pose2D pose)
{
  double delta_angle = fov_ / number_of_rays_;
  double ray_angle = pose.theta_ - (fov_ / 2);
  const auto &map_data = map_ptr_->GetMap();
  const auto &row_size = map_ptr_->GetSizeX();
  const auto &col_size = map_ptr_->GetSizeY();

  // DDA algorithm
  std::vector<Ray> rays{};
  for(ui16 i = 0; i < number_of_rays_; i++)
    {
      vector2d ray_orig{ pose.x_, pose.y_ };
      vector2d ray_dir{ std::cos(ray_angle), std::sin(ray_angle) };
      ray_dir.Norm();

      vector2d vRayUnitStepSize
          = { sqrt(1 + (ray_dir.y / ray_dir.x) * (ray_dir.y / ray_dir.x)),
              sqrt(1 + (ray_dir.x / ray_dir.y) * (ray_dir.x / ray_dir.y)) };
      vector2i vMapCheck{ static_cast<int>(ray_orig.x),
                          static_cast<int>(ray_orig.y) };
      vector2d vRayLength1D;
      vector2i vStep;

      // Establish Starting Conditions
      if(ray_dir.x < 0)
        {
          vStep.x = -1;
          vRayLength1D.x
              = (ray_orig.x - float(vMapCheck.x)) * vRayUnitStepSize.x;
        }
      else
        {
          vStep.x = 1;
          vRayLength1D.x
              = (float(vMapCheck.x + 1) - ray_orig.x) * vRayUnitStepSize.x;
        }

      if(ray_dir.y < 0)
        {
          vStep.y = -1;
          vRayLength1D.y
              = (ray_orig.y - float(vMapCheck.y)) * vRayUnitStepSize.y;
        }
      else
        {
          vStep.y = 1;
          vRayLength1D.y
              = (float(vMapCheck.y + 1) - ray_orig.y) * vRayUnitStepSize.y;
        }

      // Perform "Walk" until collision or range check
      bool bTileFound = false;
      bool is_x_side;
      float fDistance = 0.0f;
      while(!bTileFound && fDistance < ray_length_)
        {
          // Walk along shortest path
          if(vRayLength1D.x < vRayLength1D.y)
            {
              vMapCheck.x += vStep.x;
              fDistance = vRayLength1D.x;
              vRayLength1D.x += vRayUnitStepSize.x;
              is_x_side = true;
            }
          else
            {
              is_x_side = false;
              vMapCheck.y += vStep.y;
              fDistance = vRayLength1D.y;
              vRayLength1D.y += vRayUnitStepSize.y;
            }

          // Test tile at new test point
          if(vMapCheck.x >= 0 && vMapCheck.x < row_size && vMapCheck.y >= 0
             && vMapCheck.y < col_size)
            {
              if(map_data[vMapCheck.x][vMapCheck.y] != 0)
                {
                  bTileFound = true;
                }
            }
        }
      Ray intersected_ray(ray_dir);
      if(bTileFound)
        {
          intersected_ray.length_ = fDistance;
          if(is_x_side == true)
            {
              intersected_ray.perpWallDist_
                  = (vRayLength1D.x - vRayUnitStepSize.x);
              intersected_ray.is_x_side_ = true;
            }
          else
            {
              intersected_ray.perpWallDist_
                  = (vRayLength1D.y - vRayUnitStepSize.y);
              intersected_ray.is_x_side_ = false;
            }
          intersected_ray.wall_color_ = map_data[vMapCheck.x][vMapCheck.y];
        }
      rays.push_back(intersected_ray);
      ray_angle += delta_angle;
    }

  return rays;
}

} // namespace wolfenstein
