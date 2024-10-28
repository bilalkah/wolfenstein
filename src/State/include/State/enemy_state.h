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
};

typedef std::shared_ptr<EnemyState> EnemyStatePtr;

// ########################################### IdleState ###########################################
class IdleState : public EnemyState
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
class WalkState : public EnemyState
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
class AttackState : public EnemyState
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
class PainState : public EnemyState
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
class DeathState : public EnemyState
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