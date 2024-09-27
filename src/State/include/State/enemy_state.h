/**
 * @file enemy_state.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-09-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STATE_INCLUDE_STATE_ENEMY_STATE_H_
#define STATE_INCLUDE_STATE_ENEMY_STATE_H_

#include "State/state.h"
#include <vector>

namespace wolfenstein {

namespace {
constexpr double kIdlePatrolDistance = 0.5;
}  // namespace

class Enemy;
typedef std::shared_ptr<State<Enemy>> EnemyStatePtr;

template <>
struct StateType<Enemy>
{
	enum class Type { Idle, Walk, Attack, Pain, Death };
};
typedef StateType<Enemy>::Type EnemyStateType;

// ########################################### IdleState ###########################################
class IdleState : public State<Enemy>
{
  public:
	IdleState();
	~IdleState();

	void Update(const double& delta_time) override;
	void Reset() override;
	void OnContextSet() override;
	int GetCurrentFrame() const override;
	EnemyStateType GetType() const override;

  private:
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	double patrol_distance{0.25};
	std::vector<uint16_t> textures;
};

// ########################################### WalkState ###########################################
class WalkState : public State<Enemy>
{
  public:
	WalkState();
	~WalkState();

	void Update(const double& delta_time) override;
	void Reset() override;
	void OnContextSet() override;
	int GetCurrentFrame() const override;
	EnemyStateType GetType() const override;

  private:
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

// ########################################### AttackState ###########################################
class AttackState : public State<Enemy>
{
  public:
	AttackState();
	~AttackState();

	void Update(const double& delta_time) override;
	void Reset() override;
	void OnContextSet() override;
	int GetCurrentFrame() const override;
	EnemyStateType GetType() const override;

  private:
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

// ########################################### PainState ###########################################
class PainState : public State<Enemy>
{
  public:
	PainState();
	~PainState();

	void Update(const double& delta_time) override;
	void Reset() override;
	void OnContextSet() override;
	int GetCurrentFrame() const override;
	EnemyStateType GetType() const override;

  private:
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

// ########################################### DeathState ###########################################
class DeathState : public State<Enemy>
{
  public:
	DeathState();
	~DeathState();

	void Update(const double& delta_time) override;
	void Reset() override;
	void OnContextSet() override;
	int GetCurrentFrame() const override;
	EnemyStateType GetType() const override;

  private:
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_ENEMY_STATE_H_