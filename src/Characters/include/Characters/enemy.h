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

#include "Camera/single_raycaster.h"
#include "Characters/character.h"
#include "GameObjects/game_object.h"
#include "Math/vector.h"
#include "State/enemy_state.h"
#include "Strike/simple_weapon.h"
#include "Strike/strike.h"
#include <memory>
#include <string>
namespace wolfenstein {

struct AnimationTime
{
	double idle_animation_speed;
};

struct StateConfig
{
	AnimationTime animation_time;
	double follow_range_max;
	double follow_range_min;
};

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
	bool IsPlayerInShootingRange() const;
	bool IsAttacked() const;
	bool IsAlive() const;
	void Shoot();

	void SetNextPose(vector2d pose);
	void SetAttacked(bool value);
	void SetDeath();
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
	StateConfig GetStateConfig() const;
	Ray GetCrosshairRay() const;
	std::shared_ptr<SimpleWeapon> GetWeapon() const;

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
	StateConfig state_config_;
	EnemyStatePtr state_;
	std::shared_ptr<SimpleWeapon> weapon_;
	Ray crosshair_ray;
	bool is_attacked_;
	bool is_alive_;
};

class EnemyFactory
{
  public:
	static std::shared_ptr<Enemy> CreateEnemy(std::string bot_name,
											  CharacterConfig config);
};

}  // namespace wolfenstein

#endif	// CHARACTERS_INCLUDE_ENEMY_H_
