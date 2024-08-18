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

#include <Characters/character.h>

namespace wolfenstein {

class Enemy : public ICharacter, public IGameObject
{
  public:
	explicit Enemy();
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

  private:
	void Move(double delta_time);

	Position2D position_;
	std::string id_;
	int texture_id;
	double width;
	double height;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
