/**
 * @file enemy.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CHARACTERS_ENEMY_H_
#define CHARACTERS_ENEMY_H_

#include "character.h"

namespace wolfenstein {

class Enemy : public ICharacter, public IGameObject
{
  public:
	~Enemy() = default;

	void Update(double delta_time) override;

	void SetPose(const vector2d& pose) override;
	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	void SetPosition(Position2D position) override;
	Position2D GetPosition() const override;

  private:
	void Move(double delta_time);

	Position2D position_;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_ENEMY_H_
