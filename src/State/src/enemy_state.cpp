#include "State/enemy_state.h"
#include "Characters/enemy.h"
#include "NavigationManager/navigation_manager.h"
#include "ShootingManager/shooting_manager.h"
#include "TextureManager/texture_manager.h"

namespace wolfenstein {

void EnemyState::Reset() {
	animation_->Reset();
}

int EnemyState::GetCurrentFrame() const {
	return animation_->GetCurrentFrame();
}

// ########################################### IdleState ###########################################
IdleState::IdleState() {}

IdleState::~IdleState() {}

void IdleState::Update(const double& delta_time) {
	animation_->Update(delta_time);

	if (context_->IsPlayerInShootingRange()) {
		context_->TransitionTo(std::make_shared<WalkState>());
	}
	if (context_->IsAttacked()) {
		context_->TransitionTo(std::make_shared<PainState>());
	}
}

void IdleState::OnContextSet() {
	const auto config = context_->GetStateConfig();
	animation_speed_ = config.animation_time.idle_animation_speed;
	range_ = config.follow_range_max;
	animation_ = std::make_unique<TBSAnimation>(
		TextureManager::GetInstance().GetTextureCollection(
			context_->GetBotName() + "_idle"),
		animation_speed_);
}

EnemyStateType IdleState::GetType() const {
	return EnemyStateType::Idle;
}

// ########################################### WalkState ###########################################
WalkState::WalkState()
	: animation_speed_(1.2),
	  range_max_(5.0),
	  range_min_(1.5),
	  attack_range_(5.0),
	  attack_rate_(1.0),
	  attack_counter_(0.0),
	  is_attacked_(false) {}

WalkState::~WalkState() {}

void WalkState::Update(const double& delta_time) {
	const auto bot_position = context_->GetPosition();
	const auto distance =
		NavigationManager::GetInstance().EuclideanDistanceToPlayer(
			bot_position);
	if (context_->IsAttacked()) {
		NavigationManager::GetInstance().ResetPath(context_->GetId());
		context_->SetNextPose(bot_position.pose);
		context_->TransitionTo(std::make_shared<PainState>());
		return;
	}
	if (!context_->IsPlayerInShootingRange()) {
		if (distance > range_max_) {
			NavigationManager::GetInstance().ResetPath(context_->GetId());
			context_->TransitionTo(std::make_shared<IdleState>());
			return;
		}
	}
	if (distance <= attack_range_ && context_->IsPlayerInShootingRange()) {
		attack_counter_ += delta_time;
		if (attack_counter_ > attack_rate_) {
			context_->SetNextPose(bot_position.pose);
			context_->TransitionTo(std::make_shared<AttackState>());
			return;
		}
	}
	if ((!context_->IsPlayerInShootingRange()) ||
		((distance > range_min_) && context_->IsPlayerInShootingRange())) {
		auto next_position = NavigationManager::GetInstance().FindPathToPlayer(
			bot_position, context_->GetId());
		context_->SetNextPose(next_position);
	}
	else {
		NavigationManager::GetInstance().ResetPath(context_->GetId());
		context_->SetNextPose(bot_position.pose);
	}

	animation_->Update(delta_time);
}
void WalkState::OnContextSet() {
	attack_rate_ = context_->GetWeapon()->GetAttackRate();
	attack_range_ = context_->GetWeapon()->GetAttackRange();
	animation_ = std::make_unique<TBSAnimation>(
		context_->GetBotName() + "_walk", animation_speed_);
}

EnemyStateType WalkState::GetType() const {
	return EnemyStateType::Walk;
}

// ########################################### AttackState ###########################################
AttackState::AttackState() : animation_speed_(0.5), attack_counter_(0.0) {}

AttackState::~AttackState() {}

void AttackState::Update(const double& delta_time) {
	animation_->Update(delta_time);
	if (attack_counter_ == 0.0) {
		context_->Shoot();
	}
	if (context_->IsAttacked()) {
		context_->TransitionTo(std::make_shared<PainState>());
		return;
	}
	if (attack_counter_ > animation_speed_) {
		context_->TransitionTo(std::make_shared<WalkState>());
		return;
	}
	attack_counter_ += delta_time;
}

void AttackState::OnContextSet() {
	animation_speed_ = context_->GetWeapon()->GetAttackSpeed();
	animation_ = std::make_unique<TBSAnimation>(
		context_->GetBotName() + "_attack", animation_speed_);
}

EnemyStateType AttackState::GetType() const {
	return EnemyStateType::Attack;
}

// ########################################### PainState ###########################################
PainState::PainState() : animation_speed_(0.2), counter(0.0) {}

PainState::~PainState() {}

void PainState::Update(const double& delta_time) {
	animation_->Update(delta_time);
	counter += delta_time;
	if (counter > animation_speed_) {
		if (context_->GetHealth() <= 0) {
			context_->TransitionTo(std::make_shared<DeathState>());
			return;
		}
		context_->SetAttacked(false);
		context_->TransitionTo(std::make_shared<WalkState>());
	}
}

void PainState::OnContextSet() {
	animation_ = std::make_unique<TBSAnimation>(
		context_->GetBotName() + "_pain", animation_speed_);
}

EnemyStateType PainState::GetType() const {
	return EnemyStateType::Pain;
}

// ########################################### DeathState ###########################################
DeathState::DeathState() : animation_speed_(1.0) {}

DeathState::~DeathState() {}

void DeathState::Update(const double& delta_time) {
	if (!animation_->IsAnimationFinishedOnce()) {
		animation_->Update(delta_time);
		counter += delta_time;
	}
	else {
		NavigationManager::GetInstance().ResetPath(context_->GetId());
		context_->SetDeath();
	}
}

void DeathState::OnContextSet() {
	animation_ = std::make_unique<TBSAnimation>(
		context_->GetBotName() + "_death", animation_speed_);
}

EnemyStateType DeathState::GetType() const {
	return EnemyStateType::Death;
}

}  // namespace wolfenstein
