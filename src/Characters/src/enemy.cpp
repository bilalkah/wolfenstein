#include "Characters/enemy.h"
#include "CollisionManager/collision_manager.h"
#include "Math/vector.h"
#include "Utility/uuid_generator.h"
#include <memory>

namespace wolfenstein {

Enemy::Enemy(CharacterConfig config, EnemyStatePtr state,
			 double width, double height)
	: position_(config.initial_position),
	  rotation_speed_(config.rotation_speed),
	  translation_speed_(config.translation_speed),
	  state_(state),
	  width(width),
	  height(height),
	  id_(UuidGenerator::GetInstance().GenerateUuid().bytes()) {}

void Enemy::Init() {
	state_->SetContext(shared_from_this());
}

void Enemy::TransitionTo(EnemyStatePtr state) {
	state_ = state;
	state_->SetContext(shared_from_this());
}

void Enemy::Update(double delta_time) {
	state_->Update(delta_time);
	if (next_pose != position_.pose) {
		Move(delta_time);
	}
}

void Enemy::SetPose(const vector2d& pose) {
	position_.pose = pose;
}

vector2d Enemy::GetPose() const {
	return position_.pose;
}

ObjectType Enemy::GetObjectType() const {
	return ObjectType::CHARACTER_ENEMY;
}

void Enemy::SetPosition(Position2D position) {
	position_ = position;
}

Position2D Enemy::GetPosition() const {
	return position_;
}

std::string Enemy::GetId() const {
	return id_;
}

void Enemy::Move(double delta_time) {
	vector2d direction = next_pose - position_.pose;
	direction.Norm();
	vector2d delta_movement = direction * translation_speed_ * delta_time;
	if (!CollisionManager::GetInstance().CheckWallCollision(
			position_.pose, {delta_movement.x, 0})) {
		position_.pose.x += delta_movement.x;
	}
	if (!CollisionManager::GetInstance().CheckWallCollision(
			position_.pose, {0, delta_movement.y})) {
		position_.pose.y += delta_movement.y;
	}
}

void Enemy::SetNextPose(vector2d pose) {
	next_pose = pose;
}

int Enemy::GetTextureId() const {
	return state_->GetCurrentFrame();
}

double Enemy::GetWidth() const {
	return width;
}
double Enemy::GetHeight() const {
	return height;
}

}  // namespace wolfenstein
