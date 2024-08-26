#include "Characters/enemy.h"
#include "Utility/uuid_generator.h"

namespace wolfenstein {

Enemy::Enemy(CharacterConfig config, double width, double height,
			 WalkAnimation animation)
	: position_(config.initial_position),
	  rotation_speed_(config.rotation_speed),
	  translation_speed_(config.translation_speed),
	  width(width),
	  height(height),
	  animation(animation) {
	id_ = UuidGenerator::GetInstance().GenerateUuid().bytes();
}

void Enemy::Update(double delta_time) {
	if (next_pose != position_.pose) {
		Move(delta_time);
		animation.Update(delta_time);
	}
	else {
		animation.Reset();
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
	vector2d new_pose =
		position_.pose + direction * translation_speed_ * delta_time;
	position_.pose = new_pose;
}

void Enemy::SetNextPose(vector2d pose) {
	next_pose = pose;
}

int Enemy::GetTextureId() const {
	return animation.GetCurrentFrame();
}

double Enemy::GetWidth() const {
	return width;
}
double Enemy::GetHeight() const {
	return height;
}

}  // namespace wolfenstein
