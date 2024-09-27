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
#include <string>

namespace wolfenstein {

class EnemyFactory;
class Enemy : public ICharacter,
			  public IGameObject,
			  public std::enable_shared_from_this<Enemy>
{
  public:
	explicit Enemy(std::string bot_name, CharacterConfig config);
	~Enemy() = default;
	void Update(double delta_time) override;
	void TransitionTo(EnemyStatePtr state);

	void SetPose(const vector2d& pose) override;
	void SetPosition(Position2D position) override;
	void IncreaseHealth(double amount) override;
	void DecreaseHealth(double amount) override;
	double GetHealth() const override;

	ObjectType GetObjectType() const override;
	vector2d GetPose() const override;
	Position2D GetPosition() const override;
	std::string GetId() const override;
	std::string GetBotName() const;
	int GetTextureId() const override;
	double GetWidth() const override;
	double GetHeight() const override;

	void SetNextPose(vector2d pose);
	friend class EnemyFactory;

  private:
	Enemy() = default;

	void Move(double delta_time);

	std::string bot_name_;
	Position2D position_;
	double rotation_speed_;
	double translation_speed_;
	double width;
	double height;
	double health_{100};
	std::string id_;
	vector2d next_pose;
	EnemyStatePtr state_;
	EnemyStatePtr previous_state_;
};

class EnemyFactory
{
  public:
	static std::shared_ptr<Enemy> CreateEnemy(std::string bot_name,
											  CharacterConfig config);
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
