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
	  max_health_(config.health * scene.GetDifficulty().enemy_health),
	  health_(max_health_),
	  position_(position),
	  next_pose(position_.pose),
	  previous_pose_(position_.pose),
	  config_(config),
	  crosshair_ray(Ray{}),
	  weapon_(config.weapon) {
	for (const auto type :
		 {EnemyStateType::Idle, EnemyStateType::Patrol, EnemyStateType::Walk,
		  EnemyStateType::Attack, EnemyStateType::Pain, EnemyStateType::Death,
		  EnemyStateType::Retreat}) {
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
		case EnemyStateType::Retreat:
			return retreat_state_;
	}
	std::unreachable();
}

EnemyStateType Enemy::GetStateType() const {
	return state_machine_.Current().GetType();
}

void Enemy::TransitionTo(EnemyStateType type) {
	face_player_ = false;
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
		// Heard from where it stands; one sound at a time, each cutting off
		// its last
		scene_.PlaySoundAt(effect, position_.pose,
						   static_cast<std::uint32_t>(ToIndex(GetId())) + 1);
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
	sidestep_left_ = std::max(sidestep_left_ - delta_time, 0.0);
	if (scene_.GetPlayer().IsAlive()) {
		ScopedTimer timer(ProfileSection::LineOfSight);
		crosshair_ray = CastLineOfSight(scene_.GetMap(), position_.pose,
										scene_.GetPlayer().GetPosition().pose);
	}
	else {
		// The player fallen: no one to see, to hunt or to shoot
		crosshair_ray = Ray{};
		alerted_for_ = 0.0;
	}
	weapon_.SetCrosshairRay(crosshair_ray);
	state_machine_.Update(delta_time);
	if (!(next_pose == position_.pose)) {
		Move(delta_time);
	}
	if (health_ > 0.0) {
		KeepApart(delta_time);
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
	// It faces the way it walks, or, fighting, the player
	if (face_player_) {
		FacePlayer();
	}
	else {
		position_.theta = std::atan2(direction.y, direction.x);
	}
	vector2d delta_movement =
		direction * translation_speed_ * pace_ * delta_time;
	// As far as it can go: round lamps and the player (other enemies do not
	// stop it, or they would jam in doorways), then along the walls
	const Player& player = scene_.GetPlayer();
	vector2d reached = ResolveObjectCollisions(
		scene_.GetObjects(), this, position_.pose,
		position_.pose + delta_movement, radius_, /*ignore_enemies=*/true);
	if (player.IsAlive()) {
		reached = PushOutOf(player.GetPose(), player.GetWidth() / 2,
							position_.pose, reached, radius_);
	}
	Slide(reached - position_.pose);
}

void Enemy::Slide(const vector2d& step) {
	// An axis at a time, so it slides along a wall it meets
	const Map& map = scene_.GetMap();
	if (!CheckWallCollision(map, position_.pose, {step.x, 0}, radius_)) {
		position_.pose.x += step.x;
	}
	if (!CheckWallCollision(map, position_.pose, {0, step.y}, radius_)) {
		position_.pose.y += step.y;
	}
}

void Enemy::KeepApart(double delta_time) {
	// Standing in another, it eases out of it, a little each tick: a group
	// does not stand on one spot, and two can still squeeze past each other
	// in a doorway (a wall stops the easing, not them)
	constexpr double kEasePerSecond = 4.0;
	vector2d apart{0.0, 0.0};
	for (const Enemy* other : scene_.GetEnemies()) {
		if (other == this || other->GetCollisionRadius() <= 0.0) {
			continue;
		}
		const vector2d gap = position_.pose - other->GetPose();
		const double distance = gap.Magnitude();
		const double overlap = radius_ + other->GetRadius() - distance;
		if (overlap > 0.0) {
			// Exactly on it: out along the way each faces, so they part
			const vector2d away = distance > 1e-6
									  ? gap / distance
									  : vector2d{std::cos(position_.theta),
												 std::sin(position_.theta)};
			apart = apart + away * (overlap / 2);
		}
	}
	if (apart.x != 0.0 || apart.y != 0.0) {
		// Standing, it stands where it was eased to; walking, it goes on
		const bool standing = next_pose == position_.pose;
		Slide(apart * std::min(kEasePerSecond * delta_time, 1.0));
		if (standing) {
			next_pose = position_.pose;
		}
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
	return IsAlerted() ||
		   (IsPlayerInShootingRange() && scene_.GetPlayer().GetPose().Distance(
											 position_.pose) <= SightRange());
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

double Enemy::NextRandom() {
	random_ ^= random_ << 13;
	random_ ^= random_ >> 17;
	random_ ^= random_ << 5;
	return static_cast<double>(random_) / 4294967296.0;
}

void Enemy::PickWaypoint() {
	// A few tries at a spot that will do; its post if none does
	constexpr int kTries = 12;
	constexpr double kStep = 1.0;  // not a shuffle on the spot
	const Map& map = scene_.GetMap();
	for (int attempt = 0; attempt < kTries; ++attempt) {
		const double angle = 2.0 * std::numbers::pi * NextRandom();
		// Uniform over the disc
		const double reach = patrol_radius_ * std::sqrt(NextRandom());
		const vector2d spot =
			post_ + vector2d{std::cos(angle), std::sin(angle)} * reach;
		if (spot.Distance(position_.pose) >= kStep && IsOpenFloor(spot) &&
			CastLineOfSight(map, post_, spot).is_hit) {
			waypoint_ = spot;
			return;
		}
	}
	waypoint_ = post_;
}

bool Enemy::IsOpenFloor(const vector2d& at) const {
	constexpr double kClearance = 0.35;
	const Map& map = scene_.GetMap();
	for (const vector2d off :
		 {vector2d{0.0, 0.0}, vector2d{kClearance, 0.0},
		  vector2d{-kClearance, 0.0}, vector2d{0.0, kClearance},
		  vector2d{0.0, -kClearance}}) {
		if (map.IsBlocked(at + off)) {
			return false;
		}
	}
	return true;
}

vector2d Enemy::ApproachSpot() const {
	const vector2d player = scene_.GetPlayer().GetPose();
	const vector2d from = position_.pose - player;
	if (from.Magnitude() < 1e-9) {
		return player;
	}
	const double mine = std::atan2(from.y, from.x);
	const auto& tactics = config_.behaviour;
	constexpr double kInto = 0.75;	// well inside its range, not at the edge
	const double range =
		std::max(tactics.near_range, kInto * tactics.far_range);
	// How far round a bearing is from the nearest other enemy engaged
	const auto room = [&](double bearing) {
		double nearest = std::numbers::pi;
		for (const Enemy* other : scene_.GetEnemies()) {
			if (other == this || other->GetHealth() <= 0.0 || other->IsCalm()) {
				continue;
			}
			const vector2d at = other->GetPose() - player;
			nearest = std::min(nearest, std::abs(std::remainder(
											bearing - std::atan2(at.y, at.x),
											2.0 * std::numbers::pi)));
		}
		return nearest;
	};
	// Its own way in, or turned from it by up to a right angle; each turn
	// must win it more room than it costs
	constexpr std::array<double, 7> kTurns{0.0,	 0.45, -0.45, 0.9,
										   -0.9, 1.35, -1.35};
	constexpr double kTurnCost = 0.3;
	std::array<std::pair<double, double>, kTurns.size()> choices{};
	for (std::size_t i = 0; i < kTurns.size(); ++i) {
		const double bearing = mine + kTurns[i];
		choices[i] = {room(bearing) - kTurnCost * std::abs(kTurns[i]), bearing};
	}
	std::ranges::sort(choices, std::greater{});
	const Map& map = scene_.GetMap();
	for (const auto& [score, bearing] : choices) {
		const vector2d spot =
			player + vector2d{std::cos(bearing), std::sin(bearing)} * range;
		if (IsOpenFloor(spot) &&
			map.FindDoor(static_cast<int>(std::floor(spot.x)),
						 static_cast<int>(std::floor(spot.y))) == nullptr &&
			CastLineOfSight(map, spot, player).is_hit) {
			return spot;
		}
	}
	return player;
}

bool Enemy::IsBunched() const {
	constexpr double kElbowRoom = 2.0;
	return std::ranges::any_of(scene_.GetEnemies(), [&](const Enemy* other) {
		return other != this && other->GetHealth() > 0.0 && !other->IsCalm() &&
			   other->GetPose().Distance(position_.pose) < kElbowRoom;
	});
}

bool Enemy::InDoorway() const {
	return scene_.GetMap().FindDoor(
			   static_cast<int>(std::floor(position_.pose.x)),
			   static_cast<int>(std::floor(position_.pose.y))) != nullptr;
}

vector2d Enemy::BackOffSpot() const {
	const vector2d player = scene_.GetPlayer().GetPose();
	const vector2d away = position_.pose - player;
	const double length = away.Magnitude();
	if (length < 1e-9) {
		return position_.pose;
	}
	// A step at a time: it is chosen afresh every tick
	constexpr double kStep = 0.75;
	const Map& map = scene_.GetMap();
	for (const double turn : {0.0, 0.6, -0.6, 1.2, -1.2}) {
		const vector2d way{
			(away.x * std::cos(turn) - away.y * std::sin(turn)) / length,
			(away.x * std::sin(turn) + away.y * std::cos(turn)) / length};
		const vector2d spot = position_.pose + way * kStep;
		if (IsOpenFloor(spot) &&
			CastLineOfSight(map, position_.pose, spot).is_hit &&
			CastLineOfSight(map, spot, player).is_hit) {
			return spot;
		}
	}
	return position_.pose;
}

void Enemy::PlanSidestep() {
	sidestep_left_ = 0.0;
	const double step = config_.behaviour.sidestep;
	const vector2d player = scene_.GetPlayer().GetPose();
	const vector2d to = player - position_.pose;
	const double length = to.Magnitude();
	if (step <= 0.0 || length < 1e-9) {
		return;
	}
	const vector2d across{-to.y / length, to.x / length};
	// Mostly the other side from last time, now and then the same again
	constexpr double kSwitch = 0.75;
	if (NextRandom() < kSwitch) {
		sidestep_side_ = -sidestep_side_;
	}
	// Longer than the step takes at its pace, less than two shots apart
	constexpr double kMostSeconds = 1.5;
	const Map& map = scene_.GetMap();
	for (const double side : {sidestep_side_, -sidestep_side_}) {
		for (const double share : {1.0, 0.5}) {
			const vector2d spot =
				position_.pose + across * (side * step * share);
			if (IsOpenFloor(spot) &&
				CastLineOfSight(map, position_.pose, spot).is_hit &&
				CastLineOfSight(map, spot, player).is_hit) {
				sidestep_to_ = spot;
				sidestep_side_ = side;
				sidestep_left_ = kMostSeconds;
				return;
			}
		}
	}
}

bool Enemy::WantsToRetreat() const {
	return retreat_ != Retreat::Done && config_.behaviour.retreat_below > 0.0 &&
		   health_ > 0.0 &&
		   health_ <= max_health_ * config_.behaviour.retreat_below;
}

bool Enemy::FindCover() {
	if (retreat_ == Retreat::Running) {
		return true;  // where it was running already
	}
	retreat_ = Retreat::Done;
	const vector2d player = scene_.GetPlayer().GetPose();
	const double from_player = position_.pose.Distance(player);
	const Map& map = scene_.GetMap();
	// Spots round it, a cell apart out to six cells; out of the player's
	// sight, on open floor, not towards the player
	constexpr int kRings = 6;
	constexpr int kAround = 16;
	struct Spot
	{
		vector2d at;
		double run = 0.0;
	};
	std::array<Spot, std::size_t{kRings} * kAround> spots{};
	std::size_t count = 0;
	for (int ring = 1; ring <= kRings; ++ring) {
		for (int i = 0; i < kAround; ++i) {
			const double angle = 2.0 * std::numbers::pi * i / kAround;
			const vector2d at =
				position_.pose + vector2d{std::cos(angle), std::sin(angle)} *
									 static_cast<double>(ring);
			if (at.Distance(player) > from_player && IsOpenFloor(at) &&
				!CastLineOfSight(map, player, at).is_hit) {
				spots[count++] = {.at = at, .run = at.Distance(position_.pose)};
			}
		}
	}
	// The nearest it has a way to, of the first few
	std::sort(spots.begin(), spots.begin() + static_cast<std::ptrdiff_t>(count),
			  [](const Spot& a, const Spot& b) { return a.run < b.run; });
	constexpr std::size_t kTries = 3;
	for (std::size_t i = 0; i < std::min(count, kTries); ++i) {
		if (!(scene_.GetNavigation().FindPath(position_,
											  Position2D(spots[i].at, 0.0),
											  GetId()) == position_.pose)) {
			cover_ = spots[i].at;
			retreat_ = Retreat::Running;
			return true;
		}
	}
	return false;
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

bool Enemy::AddDrop(Pickup& drop) {
	if (drop_count_ == kMaxDrops) {
		return false;
	}
	drops_[drop_count_++] = &drop;
	return true;
}

void Enemy::SetDeath() {
	crosshair_ray = Ray{};
	is_alive_ = false;
	// What it carried, where it fell: side by side across the way it faced,
	// where the floor is open, else at its feet
	constexpr double kApart = 0.3;
	const vector2d across{-std::sin(position_.theta),
						  std::cos(position_.theta)};
	for (std::size_t i = 0; i < drop_count_; ++i) {
		const double off = (static_cast<double>(i) -
							static_cast<double>(drop_count_ - 1) / 2) *
						   kApart;
		const vector2d at = position_.pose + across * off;
		drops_[i]->DropAt(scene_.GetMap().IsBlocked(at) ? position_.pose : at);
	}
	drop_count_ = 0;
}

int Enemy::GetTextureId() const {
	return state_machine_.Current().GetCurrentFrame();
}

double Enemy::TurnedFrom(const vector2d& viewer) const {
	return wolfenstein::TurnedFrom(position_.pose, position_.theta, viewer);
}

std::size_t Enemy::ViewFrom(const vector2d& viewer) const {
	return SideSeen(position_.pose, position_.theta, viewer);
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
