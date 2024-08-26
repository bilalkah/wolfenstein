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

#ifndef CHARACTERS_INCLUDE_ENEMY_H_
#define CHARACTERS_INCLUDE_ENEMY_H_

#include "Animation/walk_animation.h"
#include "Characters/character.h"
#include "Math/vector.h"

namespace wolfenstein {

class Enemy : public ICharacter, public IGameObject
{
  public:
	explicit Enemy(CharacterConfig config, double width, double height,
				   WalkAnimation animation);
	~Enemy() = default;

	void Update(double delta_time) override;

	void SetPose(const vector2d& pose) override;
	void SetPosition(Position2D position) override;

	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	Position2D GetPosition() const override;
	std::string GetId() const override;
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;

	void SetNextPose(vector2d pose);

  private:
	void Move(double delta_time);

	Position2D position_;
	double rotation_speed_;
	double translation_speed_;
	std::string id_;
	double width;
	double height;
	vector2d next_pose;
	WalkAnimation animation;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
