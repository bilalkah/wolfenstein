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

#include "Characters/character.h"
#include "GameObjects/game_object.h"
#include "Math/vector.h"
#include "State/enemy_state.h"
#include <memory>

namespace wolfenstein {

class Enemy : public ICharacter,
			  public IGameObject,
			  public std::enable_shared_from_this<Enemy>
{
  public:
	explicit Enemy(CharacterConfig config, std::shared_ptr<State<Enemy>> state,
				   double width, double height);
	~Enemy() = default;
	void Init();
	void Update(double delta_time) override;
	void TransitionTo(std::shared_ptr<State<Enemy>> state);

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
	Enemy() = default;
	void Move(double delta_time);

	Position2D position_;
	double rotation_speed_;
	double translation_speed_;
	std::shared_ptr<State<Enemy>> state_;
	double width;
	double height;
	std::string id_;
	vector2d next_pose;
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
