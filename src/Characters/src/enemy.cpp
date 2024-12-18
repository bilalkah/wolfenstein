#include "Camera/ray.h"
#include "Camera/single_raycaster.h"
#include "Characters/enemy.h"
#include "CollisionManager/collision_manager.h"
#include "Math/vector.h"
#include "Strike/simple_weapon.h"
#include "Strike/strike.h"
#include "Utility/uuid_generator.h"
#include <memory>

namespace wolfenstein {

namespace {
auto GetBotStateConfig = [](const std::string& bot_name) -> StateConfig {
	if (bot_name == "soldier") {
		return {{0.8}, 5.0, 1.0};
	}
	else if (bot_name == "caco_demon") {
		return {{0.8}, 5.0, 1.0};
	}
	else if (bot_name == "cyber_demon") {
		return {{0.8}, 5.0, 1.0};
	}
	else {
		throw std::invalid_argument("Invalid bot name");
	}
};

auto GetBotWeapon =
	[](const std::string& bot_name) -> std::shared_ptr<SimpleWeapon> {
	if (bot_name == "soldier") {
		return std::move(std::make_shared<Rifle>());
	}
	else if (bot_name == "caco_demon") {
		return std::move(std::make_shared<Melee>());
	}
	else if (bot_name == "cyber_demon") {
		return std::move(std::make_shared<LaserGun>());
	}
	else {
		throw std::invalid_argument("Invalid bot name");
	}
};
}  // namespace

Enemy::Enemy(std::string bot_name, CharacterConfig config)
	: is_attacked_(false),
	  is_alive_(true),
	  rotation_speed_(config.rotation_speed),
	  translation_speed_(config.translation_speed),
	  width(config.width),
	  height(config.height),
	  health_(100),
	  position_(config.initial_position),
	  next_pose(position_.pose),
	  state_config_(GetBotStateConfig(bot_name)),
	  bot_name_(bot_name),
	  id_(UuidGenerator::GetInstance().GenerateUuid().bytes()),
	  crosshair_ray(Ray{}),
	  weapon_(GetBotWeapon(bot_name)) {}

void Enemy::TransitionTo(EnemyStatePtr state) {
	state_ = state;
	state_->SetContext(shared_from_this());
}

bool Enemy::IsPlayerInShootingRange() const {
	return crosshair_ray.is_hit;
}

bool Enemy::IsAttacked() const {
	return is_attacked_;
}

bool Enemy::IsAlive() const {
	return is_alive_;
}

void Enemy::Shoot() {
	weapon_->Attack();
}

void Enemy::Update(double delta_time) {
	if (!is_alive_) {
		return;
	}
	crosshair_ray = SingleRayCasterService::GetInstance().Cast(position_.pose);
	weapon_->SetCrosshairRay(crosshair_ray);
	state_->Update(delta_time);
	if (!(next_pose == position_.pose)) {
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

void Enemy::SetPosition(const Position2D position) {
	position_ = position;
}

void Enemy::IncreaseHealth(double amount) {
	health_ += amount;
}

void Enemy::DecreaseHealth(double amount) {
	health_ -= amount;
}

double Enemy::GetHealth() const {
	return health_;
}

Position2D Enemy::GetPosition() const {
	return position_;
}

std::string Enemy::GetId() const {
	return id_;
}

std::string Enemy::GetBotName() const {
	return bot_name_;
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

void Enemy::SetAttacked(bool value) {
	is_attacked_ = value;
}

void Enemy::SetDeath() {
	crosshair_ray = Ray{};
	is_alive_ = false;
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

StateConfig Enemy::GetStateConfig() const {
	return state_config_;
}

const Ray& Enemy::GetCrosshairRay() const {
	return crosshair_ray;
}

const SimpleWeapon& Enemy::GetWeapon() const {
	return *weapon_;
}

SimpleWeapon& Enemy::GetWeapon() {
	return *weapon_;
}

std::shared_ptr<Enemy> EnemyFactory::CreateEnemy(std::string bot_name,
												 CharacterConfig config) {
	auto enemy_ptr = std::make_shared<Enemy>(bot_name, config);
	enemy_ptr->TransitionTo(std::make_shared<IdleState>());
	return enemy_ptr;
}

}  // namespace wolfenstein
