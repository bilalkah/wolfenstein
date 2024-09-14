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

namespace wolfenstein {

class Enemy;

class IdleState : public State<Enemy>
{
  public:
	IdleState(const std::string bot_name);
	~IdleState();

	void Update(const double& delta_time) override;
	void Reset() override;
	int GetCurrentFrame() const override;

  private:
	std::string bot_name_;
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

class WalkState : public State<Enemy>
{
  public:
	WalkState(const std::string bot_name);
	~WalkState();

	void Update(const double& delta_time) override;
	void Reset() override;
	int GetCurrentFrame() const override;

  private:
	std::string bot_name_;
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

class AttackState : public State<Enemy>
{
  public:
	AttackState(const std::string bot_name);
	~AttackState();

	void Update(const double& delta_time) override;
	void Reset() override;
	int GetCurrentFrame() const override;

  private:
	std::string bot_name_;
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

class PainState : public State<Enemy>
{
  public:
	PainState(const std::string bot_name);
	~PainState();

	void Update(const double& delta_time) override;
	void Reset() override;
	int GetCurrentFrame() const override;

  private:
	std::string bot_name_;
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

class DeathState : public State<Enemy>
{
  public:
	DeathState(const std::string bot_name);
	~DeathState();

	void Update(const double& delta_time) override;
	void Reset() override;
	int GetCurrentFrame() const override;

  private:
	std::string bot_name_;
	int current_frame;
	double counter;
	double interval;
	double animation_speed_;
	std::vector<uint16_t> textures;
};

}  // namespace wolfenstein

#endif	// STATE_INCLUDE_STATE_ENEMY_STATE_H_