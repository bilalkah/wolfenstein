/**
 * @file player.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CHARACTER_PLAYER_H
#define CHARACTER_PLAYER_H

#include "camera/camera.h"
#include "character/i_character.h"
#include <memory>

namespace wolfenstein {

class Player : public ICharacter
{
  public:
	Player(const PlayerConfig config);

	void Update(const double delta_time) override;
	Position2D GetPosition() const override;
	std::shared_ptr<RayVector> GetRays() const;
	void SetCamera(const std::unique_ptr<Camera2D> camera);
	std::shared_ptr<Ray> GetCrosshairRay() const;

  private:
	void Move();
	void Rotate();

	Position2D position_;
	double rotation_speed_;
	double translation_speed_;
	std::unique_ptr<Camera2D> camera_;
	double delta_time_;
};

}  // namespace wolfenstein

#endif	// CHARACTER_PLAYER_H
