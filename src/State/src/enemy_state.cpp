#include "State/enemy_state.h"
#include "Characters/enemy.h"
#include "TextureManager/texture_manager.h"
#include <unordered_map>
namespace wolfenstein {

namespace {
const std::unordered_map<std::string,
						 std::unordered_map<std::string, std::string>>
	enemy_data_{{"soldier",
				 {{"idle", "soldier_idle"},
				  {"walk", "soldier_walk"},
				  {"attack", "soldier_attack"},
				  {"death", "soldier_death"},
				  {"pain", "soldier_pain"}}},
				{"caco_demon",
				 {{"idle", "caco_demon_idle"},
				  {"walk", "caco_demon_walk"},
				  {"attack", "caco_demon_attack"},
				  {"death", "caco_demon_death"},
				  {"pain", "caco_demon_pain"}}},
				{"cyber_demon",
				 {{"idle", "cyber_demon_idle"},
				  {"walk", "cyber_demon_walk"},
				  {"attack", "cyber_demon_attack"},
				  {"death", "cyber_demon_death"},
				  {"pain", "cyber_demon_pain"}}}};
}  // namespace

IdleState::IdleState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_.at(bot_name_).at("idle"));
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

EnemyStateType IdleState::GetType() const {
	return EnemyStateType::Idle;
}

WalkState::WalkState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_.at(bot_name_).at("walk"));
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

EnemyStateType WalkState::GetType() const {
	return EnemyStateType::Walk;
}

AttackState::AttackState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_.at(bot_name_).at("attack"));
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

EnemyStateType AttackState::GetType() const {
	return EnemyStateType::Attack;
}

PainState::PainState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_.at(bot_name_).at("pain"));
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

EnemyStateType PainState::GetType() const {
	return EnemyStateType::Pain;
}

DeathState::DeathState(const std::string bot_name)
	: bot_name_(bot_name), current_frame(0), counter(0), interval(0) {
	animation_speed_ = 0.3;
	textures = TextureManager::GetInstance().GetTextureCollection(
		enemy_data_.at(bot_name_).at("death"));
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

EnemyStateType DeathState::GetType() const {
	return EnemyStateType::Death;
}

}  // namespace wolfenstein
