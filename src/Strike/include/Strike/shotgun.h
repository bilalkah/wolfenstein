/**
 * @file shotgun.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-08-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STRIKE_INCLUDE_STRIKE_SHOTGUN_H
#define STRIKE_INCLUDE_STRIKE_SHOTGUN_H

#include "Animation/time_based_single_animation.h"
#include "Strike/weapon.h"
#include <memory>

namespace wolfenstein {

class Shotgun : public IWeapon
{
  public:
	Shotgun();
	~Shotgun() = default;

	void Attack() override;
	int GetTextureId() const;

  private:
	void AttackAnimation();
	double attack_speed_;
	double attack_damage_;
	std::shared_ptr<TBSAnimation> animation_;

	bool cooldown_;
	double last_attack_time_;
};

}  // namespace wolfenstein

#endif	// STRIKE_INCLUDE_STRIKE_SHOTGUN_H
