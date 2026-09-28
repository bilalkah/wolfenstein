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
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace wolfenstein {

Enemy::Enemy(Scene& scene, const EnemyConfig& config,
			 const Position2D& position)
	: scene_(scene),
	  is_alive_(true),
	  translation_speed_(config.translation_speed),
	  width(config.width),
	  height(config.height),
	  radius_(config.radius),
	  health_(config.health * scene.GetDifficulty().enemy_health),
	  position_(position),
	  next_pose(position_.pose),
	  previous_pose_(position_.pose),
	  config_(config),
	  sound_channel_(scene.Sound().AllocateChannel()),
	  crosshair_ray(Ray{}),
	  weapon_(config.weapon) {
	for (const auto type : {EnemyStateType::Idle, EnemyStateType::Patrol,
							EnemyStateType::Walk, EnemyStateType::Attack,
							EnemyStateType::Pain, EnemyStateType::Death}) {
		StateFor(type).SetContext(*this);
	}
	watch_theta_ = position.theta;
	state_machine_.TransitionTo(idle_state_);
}

EnemyState& Enemy::StateFor(EnemyStateType type) {
	switch (type) {
		case EnemyStateType::Idle:
			return idle_state_;
		case EnemyStateType::Patrol:
			return patrol_state_;
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
	// It faces the way it walks
	position_.theta = std::atan2(direction.y, direction.x);
	vector2d delta_movement =
		direction * translation_speed_ * pace_ * delta_time;
	// As far as it can go: round lamps and the player (other enemies do not
	// stop it, or they would jam in doorways), then along the walls
	const Map& map = scene_.GetMap();
	const Player& player = scene_.GetPlayer();
	vector2d reached = ResolveObjectCollisions(
		scene_.GetObjects(), this, position_.pose,
		position_.pose + delta_movement, radius_, /*ignore_enemies=*/true);
	if (player.IsAlive()) {
		reached = PushOutOf(player.GetPose(), player.GetWidth() / 2,
							position_.pose, reached, radius_);
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

bool Enemy::NoticesPlayer() const {
	// Heard, or seen near: nothing stands between them. It looks all round
	// (seeing only ahead left guards standing blind while the player walked
	// in behind them), whichever way it faces.
	constexpr double kFurther = 2.0;  // looking out past its follow range
	return IsAlerted() ||
		   (IsPlayerInShootingRange() &&
			scene_.GetPlayer().GetPose().Distance(position_.pose) <=
				config_.behaviour.follow_range + kFurther);
}

void Enemy::LookAround() {
	// Straight ahead, over one shoulder, ahead, over the other: most
	// players see it turn, its 8 views 45 degrees apart
	static constexpr std::array<double, 4> kTurns{0.0, 0.9, 0.0, -0.9};
	look_ = (look_ + 1) % kTurns.size();
	position_.theta = watch_theta_ + kTurns[look_];
}

void Enemy::SetPatrolRadius(double radius) {
	patrol_radius_ = radius;
	post_ = position_.pose;
	waypoint_ = post_;
	// Seeded by where it stands: each enemy wanders its own way, the same
	// every time the level is played
	const auto seed = static_cast<std::uint32_t>(
		std::lround(post_.x * 1009.0) * 7919 + std::lround(post_.y * 1013.0));
	random_ = seed == 0 ? 1 : seed;
	// It walks about from the start, never standing a moment first
	if (Patrols() && GetStateType() == EnemyStateType::Idle) {
		TransitionTo(EnemyStateType::Patrol);
	}
}

void Enemy::PickWaypoint() {
	const auto next = [this] {
		random_ ^= random_ << 13;
		random_ ^= random_ >> 17;
		random_ ^= random_ << 5;
		return static_cast<double>(random_) / 4294967296.0;	 // [0, 1)
	};
	// A few tries at a spot that will do; its post if none does
	constexpr int kTries = 12;
	constexpr double kStep = 1.0;		 // not a shuffle on the spot
	constexpr double kClearance = 0.35;	 // off the walls
	const Map& map = scene_.GetMap();
	const auto open = [&](const vector2d& at) {
		for (const vector2d off :
			 {vector2d{0.0, 0.0}, vector2d{kClearance, 0.0},
			  vector2d{-kClearance, 0.0}, vector2d{0.0, kClearance},
			  vector2d{0.0, -kClearance}}) {
			if (map.IsBlocked(at + off)) {
				return false;
			}
		}
		return true;
	};
	for (int attempt = 0; attempt < kTries; ++attempt) {
		const double angle = 2.0 * std::numbers::pi * next();
		// Uniform over the disc
		const double reach = patrol_radius_ * std::sqrt(next());
		const vector2d spot =
			post_ + vector2d{std::cos(angle), std::sin(angle)} * reach;
		if (spot.Distance(position_.pose) >= kStep && open(spot) &&
			CastLineOfSight(map, post_, spot).is_hit) {
			waypoint_ = spot;
			return;
		}
	}
	waypoint_ = post_;
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
	return health_ > 0.0 && (GetStateType() == EnemyStateType::Idle ||
							 GetStateType() == EnemyStateType::Patrol);
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

double Enemy::TurnedFrom(const vector2d& viewer) const {
	const vector2d to = viewer - position_.pose;
	return std::remainder(std::atan2(to.y, to.x) - position_.theta,
						  2.0 * std::numbers::pi);
}

std::size_t Enemy::ViewFrom(const vector2d& viewer) const {
	// Straight at the viewer is its front; facing the viewer's right (a
	// quarter turn one way) shows its left side, view 6 (Doom's rotation 7)
	const double turned = TurnedFrom(viewer);
	const auto eighths =
		static_cast<long>(std::lround(-turned / (std::numbers::pi / 4)));
	return static_cast<std::size_t>(((eighths % 8) + 8) % 8);
}

IGameObject::Appearance Enemy::SeenFrom(const vector2d& viewer) const {
	const EnemyState& state = state_machine_.Current();
	if (state.GetType() != EnemyStateType::Death) {
		return {.texture_id = state.GetFrame(ViewFrom(viewer)),
				.width = width,
				.mirrored = false};
	}
	// Seen end on, a body lying down is this much of its length
	constexpr double kEndOn = 0.4;
	const double across = std::cos(TurnedFrom(viewer));
	return {.texture_id = state.GetCurrentFrame(),
			.width = width * (kEndOn + (1.0 - kEndOn) * std::abs(across)),
			.mirrored = across < 0.0};
}

void Enemy::FacePlayer() {
	const vector2d to = scene_.GetPlayer().GetPose() - position_.pose;
	if (to.x != 0.0 || to.y != 0.0) {
		position_.theta = std::atan2(to.y, to.x);
	}
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
