#include "State/enemy_state.h"
#include "Characters/enemy.h"
#include "TextureManager/texture_manager.h"

namespace wolfenstein {

// ########################################### IdleState ###########################################
IdleState::IdleState() : current_frame(0), counter(0), interval(0) {}

IdleState::~IdleState() {}

void IdleState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<WalkState>());
	}
}

void IdleState::Reset() {
	current_frame = 0;
	counter = 0;
}

void IdleState::OnContextSet() {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		context_->GetBotName() + "_idle");
}

int IdleState::GetCurrentFrame() const {
	return textures[current_frame];
}

EnemyStateType IdleState::GetType() const {
	return EnemyStateType::Idle;
}

// ########################################### WalkState ###########################################
WalkState::WalkState() : current_frame(0), counter(0), interval(0) {}

WalkState::~WalkState() {}

void WalkState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<AttackState>());
	}
}

void WalkState::Reset() {
	current_frame = 0;
	counter = 0;
}

void WalkState::OnContextSet() {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		context_->GetBotName() + "_walk");
}

int WalkState::GetCurrentFrame() const {
	return textures[current_frame];
}

EnemyStateType WalkState::GetType() const {
	return EnemyStateType::Walk;
}

// ########################################### AttackState ###########################################
AttackState::AttackState() : current_frame(0), counter(0), interval(0) {}

AttackState::~AttackState() {}

void AttackState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<PainState>());
	}
}

void AttackState::Reset() {
	current_frame = 0;
	counter = 0;
}

void AttackState::OnContextSet() {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		context_->GetBotName() + "_attack");
}

int AttackState::GetCurrentFrame() const {
	return textures[current_frame];
}

EnemyStateType AttackState::GetType() const {
	return EnemyStateType::Attack;
}

// ########################################### PainState ###########################################
PainState::PainState() : current_frame(0), counter(0), interval(0) {}

PainState::~PainState() {}

void PainState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<DeathState>());
	}
}

void PainState::Reset() {
	current_frame = 0;
	counter = 0;
}

void PainState::OnContextSet() {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		context_->GetBotName() + "_pain");
}

int PainState::GetCurrentFrame() const {
	return textures[current_frame];
}

EnemyStateType PainState::GetType() const {
	return EnemyStateType::Pain;
}

// ########################################### DeathState ###########################################
DeathState::DeathState() : current_frame(0), counter(0), interval(0) {}

DeathState::~DeathState() {}

void DeathState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<IdleState>());
	}
}

void DeathState::Reset() {
	current_frame = 0;
	counter = 0;
}

void DeathState::OnContextSet() {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		context_->GetBotName() + "_death");
}

int DeathState::GetCurrentFrame() const {
	return textures[current_frame];
}

EnemyStateType DeathState::GetType() const {
	return EnemyStateType::Death;
}

}  // namespace wolfenstein
