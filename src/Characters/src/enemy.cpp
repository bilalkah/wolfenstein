#include <Characters/enemy.h>
#include <Utility/uuid_generator.h>

namespace wolfenstein {

Enemy::Enemy() {
	id_ = UuidGenerator::GetInstance().GenerateUuid().bytes();
}

void Enemy::Update(double delta_time) {
	Move(delta_time);
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
	(void)delta_time;
}

}  // namespace wolfenstein
