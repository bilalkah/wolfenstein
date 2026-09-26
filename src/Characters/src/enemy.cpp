#include "Characters/enemy.h"
#include "Camera/ray.h"
#include "Camera/single_raycaster.h"
#include "Characters/player.h"
#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include "Math/vector.h"
#include "Profiler/profiler.h"
#include "ShootingManager/shooting_manager.h"
#include "SoundManager/sound_manager.h"
#include <utility>

namespace wolfenstein {

Enemy::Enemy(Scene& scene, const EnemyConfig& config,
			 const Position2D& position)
	: scene_(scene),
	  is_alive_(true),
	  translation_speed_(config.translation_speed),
	  width(config.width),
	  height(config.height),
	  health_(100 * scene.GetDifficulty().enemy_health),
	  position_(position),
	  next_pose(position_.pose),
	  previous_pose_(position_.pose),
	  config_(config),
	  sound_channel_(scene.Sound().AllocateChannel()),
	  crosshair_ray(Ray{}),
	  weapon_(config.weapon) {
	for (const auto type :
		 {EnemyStateType::Idle, EnemyStateType::Walk, EnemyStateType::Attack,
		  EnemyStateType::Pain, EnemyStateType::Death}) {
		StateFor(type).SetContext(*this);
	}
	state_machine_.TransitionTo(idle_state_);
}

EnemyState& Enemy::StateFor(EnemyStateType type) {
	switch (type) {
		case EnemyStateType::Idle:
			return idle_state_;
		case EnemyStateType::Walk:
			return walk_state_;
		case EnemyStateType::Attack:
			return attack_state_;
		case EnemyStateType::Pain:
			return pain_state_;
		case EnemyStateType::Death:
			return death_state_;
	}
	std::unreachable();
}

EnemyStateType Enemy::GetStateType() const {
	return state_machine_.Current().GetType();
}

void Enemy::TransitionTo(EnemyStateType type) {
	state_machine_.TransitionTo(StateFor(type));
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

void Enemy::PlaySound(SoundEffect effect) {
	if (!silent_) {
		scene_.Sound().PlayEffect(sound_channel_, effect);
	}
}

void Enemy::Shoot() {
	ResolveEnemyShot(scene_.GetPlayer(), weapon_,
					 scene_.GetDifficulty().enemy_damage);
}

void Enemy::Update(double delta_time) {
	previous_pose_ = position_.pose;
	if (!is_alive_) {
		return;
	}
	{
		ScopedTimer timer(ProfileSection::LineOfSight);
		crosshair_ray = CastLineOfSight(scene_.GetMap(), position_.pose,
										scene_.GetPlayer().GetPosition().pose);
	}
	weapon_.SetCrosshairRay(crosshair_ray);
	state_machine_.Update(delta_time);
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

vector2d Enemy::GetRenderPose(double alpha) const {
	return Interpolate(previous_pose_, position_.pose, alpha);
}

ObjectType Enemy::GetObjectType() const {
	return ObjectType::CHARACTER_ENEMY;
}

void Enemy::SetPosition(const Position2D position) {
	position_ = position;
	previous_pose_ = position.pose;
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

const std::string& Enemy::GetBotName() const {
	return config_.type;
}

void Enemy::Move(double delta_time) {
	vector2d direction = next_pose - position_.pose;
	direction.Norm();
	vector2d delta_movement = direction * translation_speed_ * delta_time;
	const Map& map = scene_.GetMap();
	if (!CheckWallCollision(map, position_.pose, {delta_movement.x, 0})) {
		position_.pose.x += delta_movement.x;
	}
	if (!CheckWallCollision(map, position_.pose, {0, delta_movement.y})) {
		position_.pose.y += delta_movement.y;
	}
}

void Enemy::SetNextPose(vector2d pose) {
	next_pose = pose;
}

void Enemy::SetAttacked(bool value) {
	is_attacked_ = value;
}

void Enemy::RestoreDead() {
	silent_ = true;
	health_ = 0.0;
	TransitionTo(EnemyStateType::Death);
	// The death animation moves a frame an update; long updates play it out
	constexpr double kLongStep = 10.0;
	for (int step = 0; step < 256 && is_alive_; ++step) {
		state_machine_.Update(kLongStep);
	}
	silent_ = false;
}

bool Enemy::IsCalm() const {
	if (!is_alive_) {
		return true;
	}
	return health_ > 0.0 && GetStateType() == EnemyStateType::Idle;
}

void Enemy::SetDeath() {
	crosshair_ray = Ray{};
	is_alive_ = false;
}

int Enemy::GetTextureId() const {
	return state_machine_.Current().GetCurrentFrame();
}

double Enemy::GetWidth() const {
	return width;
}
double Enemy::GetHeight() const {
	return height;
}

const Ray& Enemy::GetCrosshairRay() const {
	return crosshair_ray;
}

const SimpleWeapon& Enemy::GetWeapon() const {
	return weapon_;
}

}  // namespace wolfenstein
