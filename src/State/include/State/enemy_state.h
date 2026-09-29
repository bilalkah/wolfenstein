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
#include <cstdint>
#include <memory>
#include <vector>

namespace wolfenstein {

class Enemy;

template <>
struct StateType<Enemy>
{
	enum class Type : std::uint8_t {
		Idle,
		Walk,
		Attack,
		Pain,
		Death,
		Patrol,
		Retreat
	};
};
using EnemyStateType = StateType<Enemy>::Type;

class EnemyState : public State<Enemy>
{
  public:
	~EnemyState() override = default;

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
	// The current frame seen from `view` (LoopedAnimation::GetFrame)
	int GetFrame(std::size_t view) const { return animation_.GetFrame(view); }

  protected:
	LoopedAnimation animation_;
};

// ########################################### IdleState ###########################################
class IdleState : public EnemyState
{
  public:
	void Update(const double& delta_time) override;
	void OnContextSet() override;
	void OnEnter() override;
	EnemyStateType GetType() const override;

  private:
	// A guard looks one way, then another, this often
	static constexpr double kLookSeconds = 2.2;
	double animation_speed_{0.0};
	double looking_for_{0.0};  // seconds since it last looked about
};

// ########################################### PatrolState ###########################################
// Walking about near its post, spot to spot, at its patrol pace, until it
// notices the player (or is shot)
class PatrolState : public EnemyState
{
  public:
	void Update(const double& delta_time) override;
	void OnContextSet() override;
	void OnEnter() override;
	EnemyStateType GetType() const override;

  private:
	// A stroll: its walk, slower
	double animation_speed_{1.8};
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

// ########################################### RetreatState ###########################################
// Badly hurt, it runs for the cover it chose, out of the player's sight,
// and waits there a while; then it comes back to fight to the end. Found
// there, or caught close on the way, it fights at once.
class RetreatState : public EnemyState
{
  public:
	// How long it hides
	static constexpr double kHideSeconds = 4.0;

	void Update(const double& delta_time) override;
	void OnContextSet() override;
	void OnEnter() override;
	EnemyStateType GetType() const override;

  private:
	double animation_speed_{1.0};  // running: its walk, quicker
	double hidden_for_{0.0};
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_ENEMY_STATE_H_