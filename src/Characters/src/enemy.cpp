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
	  health_(config.health * scene.GetDifficulty().enemy_health),
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
	// The others within earshot come to the fight
	scene_.MakeNoise(position_.pose, weapon_.GetNoiseRange());
}

void Enemy::Update(double delta_time) {
	previous_pose_ = position_.pose;
	if (!is_alive_) {
		return;
	}
	since_flinch_ += delta_time;
	alerted_for_ = std::max(alerted_for_ - delta_time, 0.0);
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
	// As far as it can go: round lamps and the player (other enemies do not
	// stop it, or they would jam in doorways), then along the walls
	const Map& map = scene_.GetMap();
	const Player& player = scene_.GetPlayer();
	vector2d reached = ResolveObjectCollisions(
		scene_.GetObjects(), this, position_.pose,
		position_.pose + delta_movement, width / 2, /*ignore_enemies=*/true);
	if (player.IsAlive()) {
		reached = PushOutOf(player.GetPose(), player.GetWidth() / 2,
							position_.pose, reached, width / 2);
	}
	const vector2d step = reached - position_.pose;
	if (!CheckWallCollision(map, position_.pose, {step.x, 0})) {
		position_.pose.x += step.x;
	}
	if (!CheckWallCollision(map, position_.pose, {0, step.y})) {
		position_.pose.y += step.y;
	}
}

void Enemy::SetNextPose(vector2d pose) {
	next_pose = pose;
}

void Enemy::SetAttacked(bool value) {
	is_attacked_ = value;
}

bool Enemy::TakeHit() {
	if (!is_attacked_) {
		return false;
	}
	is_attacked_ = false;
	if (health_ > 0.0 &&
		since_flinch_ < config_.behaviour.pain_cooldown_seconds) {
		return false;
	}
	since_flinch_ = 0.0;
	retaliating_ = true;
	return true;
}

void Enemy::Alert() {
	alerted_for_ = config_.behaviour.alert_seconds;
}

bool Enemy::TakeRetaliation() {
	return std::exchange(retaliating_, false);
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
	if (drop_ != nullptr) {
		drop_->DropAt(position_.pose);
		drop_ = nullptr;
	}
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
