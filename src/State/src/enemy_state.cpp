#include "State/enemy_state.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "NavigationManager/navigation_manager.h"
#include "Profiler/profiler.h"
#include "SoundManager/sound_manager.h"
#include "TextureManager/texture_manager.h"

namespace wolfenstein {

void EnemyState::Reset() {
	animation_.Reset();
}

int EnemyState::GetCurrentFrame() const {
	return animation_.GetCurrentFrame();
}

// ########################################### IdleState ###########################################

void IdleState::Update(const double& delta_time) {
	animation_.Update(delta_time);

	if (context_->IsPlayerInShootingRange() &&
		context_->GetScene().GetNavigation().EuclideanDistanceToPlayer(
			context_->GetPosition()) <= range_ + 2.0) {
		context_->TransitionTo(EnemyStateType::Walk);
		return;
	}
	if (context_->IsAttacked()) {
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
}

void IdleState::OnContextSet() {
	const auto& config = context_->GetStateConfig();
	animation_speed_ = config.idle_frame_seconds;
	range_ = config.follow_range;
	// Idle holds each frame for animation_speed_, where other clips spread
	// their duration over all frames
	animation_ =
		LoopedAnimation(LoopedAnimation::Clip(context_->GetScene().Textures(),
											  context_->GetBotName(), "idle"),
						animation_speed_);
}

EnemyStateType IdleState::GetType() const {
	return EnemyStateType::Idle;
}

// ########################################### WalkState ###########################################

void WalkState::Update(const double& delta_time) {
	const auto& bot_position = context_->GetPosition();
	const auto distance =
		context_->GetScene().GetNavigation().EuclideanDistanceToPlayer(
			bot_position);
	if (context_->IsAttacked()) {
		context_->GetScene().GetNavigation().ResetPath(context_->GetId());
		context_->SetNextPose(bot_position.pose);
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
	if (!context_->IsPlayerInShootingRange()) {
		if (distance > range_max_) {
			context_->GetScene().GetNavigation().ResetPath(context_->GetId());
			context_->TransitionTo(EnemyStateType::Idle);
			return;
		}
	}
	if (distance <= attack_range_ && context_->IsPlayerInShootingRange()) {
		attack_counter_ += delta_time;
		if (attack_counter_ > attack_rate_) {
			context_->SetNextPose(bot_position.pose);
			context_->TransitionTo(EnemyStateType::Attack);
			return;
		}
	}
	if ((!context_->IsPlayerInShootingRange()) ||
		((distance > range_min_) && context_->IsPlayerInShootingRange())) {
		ScopedTimer timer(ProfileSection::Pathfinding);
		auto next_position =
			context_->GetScene().GetNavigation().FindPathToPlayer(
				bot_position, context_->GetId());
		context_->SetNextPose(next_position);
	}
	else {
		context_->GetScene().GetNavigation().ResetPath(context_->GetId());
		context_->SetNextPose(bot_position.pose);
	}

	animation_.Update(delta_time);
}
void WalkState::OnContextSet() {
	attack_rate_ = context_->GetWeapon().GetAttackRate();
	attack_range_ = context_->GetWeapon().GetAttackRange();
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"walk", animation_speed_);
}

void WalkState::OnEnter() {
	EnemyState::OnEnter();
	attack_counter_ = 0.0;
	is_attacked_ = false;
}

EnemyStateType WalkState::GetType() const {
	return EnemyStateType::Walk;
}

// ########################################### AttackState ###########################################

void AttackState::Update(const double& delta_time) {
	animation_.Update(delta_time);
	if (attack_counter_ == 0.0) {
		context_->Shoot();
	}
	if (context_->IsAttacked()) {
		context_->TransitionTo(EnemyStateType::Pain);
		return;
	}
	if (attack_counter_ > animation_speed_) {
		context_->TransitionTo(EnemyStateType::Walk);
		return;
	}
	attack_counter_ += delta_time;
}

void AttackState::OnContextSet() {
	animation_speed_ = context_->GetWeapon().GetAttackSpeed();
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"attack", animation_speed_);
}

void AttackState::OnEnter() {
	EnemyState::OnEnter();
	attack_counter_ = 0.0;
	context_->PlaySound(SoundEffect::NpcAttack);
}

EnemyStateType AttackState::GetType() const {
	return EnemyStateType::Attack;
}

// ########################################### PainState ###########################################

void PainState::Update(const double& delta_time) {
	animation_.Update(delta_time);
	counter += delta_time;
	if (counter > animation_speed_) {
		if (context_->GetHealth() <= 0) {
			context_->GetScene().GetNavigation().ResetPath(context_->GetId());
			context_->TransitionTo(EnemyStateType::Death);
			return;
		}
		context_->SetAttacked(false);
		context_->TransitionTo(EnemyStateType::Walk);
		return;
	}
}

void PainState::OnContextSet() {
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"pain", animation_speed_);
}

void PainState::OnEnter() {
	EnemyState::OnEnter();
	counter = 0.0;
	context_->PlaySound(SoundEffect::NpcPain);
}

EnemyStateType PainState::GetType() const {
	return EnemyStateType::Pain;
}

// ########################################### DeathState ###########################################

void DeathState::Update(const double& delta_time) {
	if (animation_.IsAnimationFinishedOnce()) {
		if (context_->IsAlive()) {
			context_->SetDeath();
		}
		return;
	}
	animation_.Update(delta_time);
	counter += delta_time;
}

void DeathState::OnContextSet() {
	animation_ =
		LoopedAnimation(context_->GetScene().Textures(), context_->GetBotName(),
						"death", animation_speed_);
}

void DeathState::OnEnter() {
	EnemyState::OnEnter();
	counter = 0.0;
	context_->PlaySound(SoundEffect::NpcDeath);
}

EnemyStateType DeathState::GetType() const {
	return EnemyStateType::Death;
}

}  // namespace wolfenstein
