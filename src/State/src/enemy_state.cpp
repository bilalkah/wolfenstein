#include "State/enemy_state.h"
#include "Characters/enemy.h"
#include "TextureManager/texture_manager.h"
#include <iostream>
namespace wolfenstein {

IdleState::IdleState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_[bot_name_]["idle"]);
}

IdleState::~IdleState() {}

void IdleState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<WalkState>(bot_name_));
	}
}

void IdleState::Reset() {
	current_frame = 0;
	counter = 0;
}

int IdleState::GetCurrentFrame() const {
	return textures[current_frame];
}

WalkState::WalkState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_[bot_name_]["walk"]);
}

WalkState::~WalkState() {}

void WalkState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<AttackState>(bot_name_));
	}
}

void WalkState::Reset() {
	current_frame = 0;
	counter = 0;
}

int WalkState::GetCurrentFrame() const {
	return textures[current_frame];
}

AttackState::AttackState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_[bot_name_]["attack"]);
}

AttackState::~AttackState() {}

void AttackState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<PainState>(bot_name_));
	}
}

void AttackState::Reset() {
	current_frame = 0;
	counter = 0;
}

int AttackState::GetCurrentFrame() const {
	return textures[current_frame];
}

PainState::PainState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_[bot_name_]["pain"]);
}

PainState::~PainState() {}

void PainState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<DeathState>(bot_name_));
	}
}

void PainState::Reset() {
	current_frame = 0;
	counter = 0;
}

int PainState::GetCurrentFrame() const {
	return textures[current_frame];
}

DeathState::DeathState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_[bot_name_]["death"]);
}

DeathState::~DeathState() {}

void DeathState::Update(const double& delta_time) {
	counter += delta_time;
	if (counter > animation_speed_) {
		current_frame = (current_frame + 1) % textures.size();
		counter = 0;
	}
	interval += delta_time;
	if (interval > 5) {
		context_->TransitionTo(std::make_shared<IdleState>(bot_name_));
	}
}

void DeathState::Reset() {
	current_frame = 0;
	counter = 0;
}

int DeathState::GetCurrentFrame() const {
	return textures[current_frame];
}

}  // namespace wolfenstein
