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

#include "Animation/time_based_single_animation.h"
#include "State/state.h"
#include <memory>
#include <vector>

namespace wolfenstein {

class Enemy;

template <>
struct StateType<Enemy>
{
	enum class Type { Idle, Walk, Attack, Pain, Death };
};
typedef StateType<Enemy>::Type EnemyStateType;

class EnemyState : public State<Enemy>
{
  public:
	virtual ~EnemyState() = default;

	void Reset() override;
	int GetCurrentFrame() const override;

  protected:
	std::unique_ptr<TBSAnimation> animation_;
};

typedef std::shared_ptr<EnemyState> EnemyStatePtr;

// ########################################### IdleState ###########################################
class IdleState : public EnemyState
{
  public:
	IdleState();
	~IdleState();

	void Update(const double& delta_time) override;
	void OnContextSet() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_;
	double range_;
};

// ########################################### WalkState ###########################################
class WalkState : public EnemyState
{
  public:
	WalkState();
	~WalkState();

	void Update(const double& delta_time) override;
	void OnContextSet() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_;
	double range_max_;
	double range_min_;
	double attack_range_;
	double attack_rate_;
	double attack_counter_;
	bool is_attacked_;
};

// ########################################### AttackState ###########################################
class AttackState : public EnemyState
{
  public:
	AttackState();
	~AttackState();

	void Update(const double& delta_time) override;
	void OnContextSet() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_;
	double attack_counter_;
};

// ########################################### PainState ###########################################
class PainState : public EnemyState
{
  public:
	PainState();
	~PainState();

	void Update(const double& delta_time) override;
	void OnContextSet() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_;
	double counter;
};

// ########################################### DeathState ###########################################
class DeathState : public EnemyState
{
  public:
	DeathState();
	~DeathState();

	void Update(const double& delta_time) override;
	void OnContextSet() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_;
	double counter;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_ENEMY_STATE_H_