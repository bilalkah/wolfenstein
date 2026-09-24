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

#include "Animation/looped_animation.h"
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

  protected:
	// Copies and moves only through derived classes: copying through the
	// base would slice off the derived part
	EnemyState() = default;
	EnemyState(const EnemyState&) = default;
	EnemyState& operator=(const EnemyState&) = default;
	EnemyState(EnemyState&&) = default;
	EnemyState& operator=(EnemyState&&) = default;

  public:
	void Reset() override;
	int GetCurrentFrame() const override;

  protected:
	std::unique_ptr<LoopedAnimation> animation_;
};

// ########################################### IdleState ###########################################
class IdleState : public EnemyState
{
  public:
	void Update(const double& delta_time) override;
	void OnContextSet() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_{0.0};
	double range_{0.0};
};

// ########################################### WalkState ###########################################
class WalkState : public EnemyState
{
  public:
	void Update(const double& delta_time) override;
	void OnContextSet() override;
	void OnEnter() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_{1.2};
	double range_max_{5.0};
	double range_min_{1.5};
	double attack_range_{5.0};
	double attack_rate_{1.0};
	double attack_counter_{0.0};
	bool is_attacked_{false};
};

// ########################################### AttackState ###########################################
class AttackState : public EnemyState
{
  public:
	void Update(const double& delta_time) override;
	void OnContextSet() override;
	void OnEnter() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_{0.5};
	double attack_counter_{0.0};
};

// ########################################### PainState ###########################################
class PainState : public EnemyState
{
  public:
	void Update(const double& delta_time) override;
	void OnContextSet() override;
	void OnEnter() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_{0.2};
	double counter{0.0};
};

// ########################################### DeathState ###########################################
class DeathState : public EnemyState
{
  public:
	void Update(const double& delta_time) override;
	void OnContextSet() override;
	void OnEnter() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_{1.0};
	double counter{0.0};
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_ENEMY_STATE_H_