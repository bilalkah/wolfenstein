#include "Core/scene.h"
#include "Camera/single_raycaster.h"
#include "Profiler/profiler.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
namespace wolfenstein {

namespace {

// Size of the level arena: the pools' storage and bookkeeping, the object
// lists and the navigation data, each with room for alignment padding.
// Exceeding it throws.
std::size_t LevelArenaBytes(const Map& map, SceneCapacity capacity) {
	constexpr std::size_t kSlack = 256;
	constexpr std::size_t kBookkeeping =
		sizeof(std::uint32_t) * 2 + sizeof(std::uint8_t);
	const std::size_t enemies = capacity.enemies;
	const std::size_t pickups = capacity.pickups;
	const std::size_t objects = capacity.enemies + capacity.dynamic_objects +
								capacity.pickups + Scene::kEffects +
								Scene::kProjectiles;
	return enemies * (sizeof(Enemy) + alignof(Enemy) + kBookkeeping) +
		   capacity.dynamic_objects *
			   (sizeof(DynamicObject) + alignof(DynamicObject) + kBookkeeping) +
		   pickups * (sizeof(Pickup) + alignof(Pickup) + kBookkeeping) +
		   objects * sizeof(IGameObject*) + enemies * sizeof(Enemy*) +
		   pickups * sizeof(Pickup*) +
		   std::size_t{map.GetSizeX()} * map.GetSizeY() *
			   (1 + sizeof(std::uint16_t) +
				sizeof(std::uint32_t)) +  // explored flags, noise buffers
		   NavigationManager::MemoryFor(map.GetSizeX(), map.GetSizeY(), objects,
										enemies) +
		   map.MemoryBytes() + 8 * kSlack;
}

}  // namespace

std::size_t Scene::MemoryFor(const Map& map, SceneCapacity capacity) {
	return LevelArenaBytes(map, capacity) +
		   map.GetDoors().size() * sizeof(DoorMotion) + alignof(DoorMotion) +
		   capacity.secrets * sizeof(PushWall) + alignof(PushWall);
}

Scene::Scene(const TextureManager& textures, SoundManager& sound,
			 const Map& map, SceneCapacity capacity,
			 memory::MonotonicArena& arena)
	: textures_(textures),
	  sound_(sound),
	  arena_(arena),
	  map_(map, &arena_),
	  enemies_(capacity.enemies, &arena_),
	  dynamic_objects_(capacity.dynamic_objects, &arena_),
	  pickups_(capacity.pickups, &arena_),
	  objects_(&arena_),
	  enemy_list_(&arena_),
	  pickup_list_(&arena_),
	  explored_(std::size_t{map.GetSizeX()} * map.GetSizeY(), 0, &arena_),
	  noise_distance_(std::size_t{map.GetSizeX()} * map.GetSizeY(), kUnheard,
					  &arena_),
	  noise_queue_(std::size_t{map.GetSizeX()} * map.GetSizeY(), 0, &arena_),
	  doors_(map.GetDoors().size(), DoorMotion{}, &arena_),
	  door_channel_(sound.AllocateChannel()),
	  burst_channel_(sound.AllocateChannel()) {
	map_.ReservePushWalls(capacity.secrets);
	objects_.reserve(capacity.enemies + capacity.dynamic_objects +
					 capacity.pickups + kEffects + kProjectiles);
	const int size_x = map_.GetSizeX();
	const int size_y = map_.GetSizeY();
	for (int x = 0; x < size_x; ++x) {
		for (int y = 0; y < size_y; ++y) {
			open_cells_ += map_.IsWall(x, y) ? 0 : 1;
		}
	}
	enemy_list_.reserve(capacity.enemies);
	pickup_list_.reserve(capacity.pickups);
}

std::expected<memory::Handle<Enemy>, memory::PoolError> Scene::AddEnemy(
	const EnemyConfig& config, const Position2D& position) {
	auto handle = enemies_.Create(*this, config, position);
	if (handle) {
		Enemy* enemy = enemies_.Get(*handle);
		enemy->SetId(ObjectId{static_cast<std::uint32_t>(objects_.size())});
		objects_.push_back(enemy);
		enemy_list_.push_back(enemy);
		++number_of_alive_enemies;
	}
	return handle;
}

std::expected<memory::Handle<DynamicObject>, memory::PoolError>
Scene::AddDynamicObject(const vector2d& pose, const LoopedAnimation& animation,
						double width, double height, double radius) {
	auto handle =
		dynamic_objects_.Create(pose, animation, width, height, radius);
	if (handle) {
		DynamicObject* object = dynamic_objects_.Get(*handle);
		object->SetId(ObjectId{static_cast<std::uint32_t>(objects_.size())});
		objects_.push_back(object);
	}
	return handle;
}

std::expected<memory::Handle<Pickup>, memory::PoolError> Scene::AddPickup(
	const vector2d& pose, int texture_id, double width, double height,
	const PickupEffect& effect) {
	auto handle = pickups_.Create(pose, texture_id, width, height, effect);
	if (handle) {
		Pickup* pickup = pickups_.Get(*handle);
		pickup->SetId(ObjectId{static_cast<std::uint32_t>(objects_.size())});
		objects_.push_back(pickup);
		pickup_list_.push_back(pickup);
	}
	return handle;
}

void Scene::SetPlayer(Player& player) {
	player_ = &player;
	player.EnterScene(*this);
}

void Scene::FinishLoading() {
	blood_frames_ = textures_.GetTextureCollection("blood_puff");
	dust_frames_ = textures_.GetTextureCollection("dust_puff");
	for (Effect& effect : effects_) {
		effect.SetId(ObjectId{static_cast<std::uint32_t>(objects_.size())});
		objects_.push_back(&effect);
	}
	for (Projectile& projectile : projectiles_) {
		projectile.SetId(ObjectId{static_cast<std::uint32_t>(objects_.size())});
		objects_.push_back(&projectile);
	}
	navigation_.Build();
}

void Scene::MakeNoise(const vector2d& pose, int range) {
	if (range <= 0) {
		return;
	}
	const int size_x = map_.GetSizeX();
	const int size_y = map_.GetSizeY();
	const auto index = [size_y](int x, int y) {
		return static_cast<std::uint32_t>(x * size_y + y);
	};
	const int start_x = static_cast<int>(std::floor(pose.x));
	const int start_y = static_cast<int>(std::floor(pose.y));
	if (!map_.Contains(start_x, start_y)) {
		return;
	}
	std::ranges::fill(noise_distance_, kUnheard);
	// Breadth first, a cell a step: the steps a sound takes round walls
	std::size_t head = 0;
	std::size_t tail = 0;
	noise_distance_[index(start_x, start_y)] = 0;
	noise_queue_[tail++] = index(start_x, start_y);
	while (head < tail) {
		const std::uint32_t cell = noise_queue_[head++];
		const int x = static_cast<int>(cell) / size_y;
		const int y = static_cast<int>(cell) % size_y;
		const std::uint16_t distance = noise_distance_[cell];
		if (std::cmp_greater_equal(distance, range)) {
			continue;
		}
		for (const auto [nx, ny] : {std::pair{x + 1, y}, std::pair{x - 1, y},
									std::pair{x, y + 1}, std::pair{x, y - 1}}) {
			if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y ||
				map_.IsBlocked(nx, ny)) {
				continue;
			}
			std::uint16_t& heard = noise_distance_[index(nx, ny)];
			if (heard == kUnheard) {
				heard = static_cast<std::uint16_t>(distance + 1);
				noise_queue_[tail++] = index(nx, ny);
			}
		}
	}
	for (Enemy* enemy : enemy_list_) {
		const vector2d at = enemy->GetPose();
		const int x = static_cast<int>(std::floor(at.x));
		const int y = static_cast<int>(std::floor(at.y));
		if (enemy->IsAlive() && map_.Contains(x, y) &&
			noise_distance_[index(x, y)] != kUnheard) {
			enemy->Alert();
		}
	}
}

void Scene::ShowImpact(Impact impact, const vector2d& pose, double height,
					   double scale) {
	// Drawn a square 0.3 of a wall across, the puff at its middle, raised
	// to where the shot struck (never into the floor)
	constexpr double kFrameSeconds = 0.06;
	constexpr double kSize = 0.3;
	const double size = kSize * scale;
	effects_[next_effect_].Start(
		pose, impact == Impact::Blood ? blood_frames_ : dust_frames_,
		kFrameSeconds, size, size, std::max(height - size / 2, 0.0));
	next_effect_ = (next_effect_ + 1) % kEffects;
}

bool Scene::Wound(Enemy& enemy, double damage) {
	if (!enemy.IsAlive() || enemy.GetHealth() <= 0.0) {
		return false;
	}
	enemy.DecreaseHealth(damage);
	enemy.SetAttacked(true);
	// Hit, it cries out, killed or not: the others near it hear, however
	// far away the shot came from, and a shot from afar takes no one
	// unawares twice
	MakeNoise(enemy.GetPose(), enemy.GetStateConfig().cry_range);
	if (enemy.GetHealth() <= 0.0) {
		DecreaseAliveEnemies();
	}
	return true;
}

bool Scene::MayAttack(const Enemy& enemy) const {
	const auto attacking =
		std::ranges::count_if(enemy_list_, [&](const Enemy* other) {
			return other != &enemy &&
				   other->GetStateType() == EnemyStateType::Attack;
		});
	return attacking < difficulty_.attackers;
}

void Scene::Launch(const ProjectileConfig& config, const vector2d& from,
				   double theta, double damage) {
	constexpr double kFlightCycleSeconds = 0.2;
	Projectile& projectile = projectiles_[next_projectile_];
	next_projectile_ = (next_projectile_ + 1) % kProjectiles;
	projectile.Launch(
		config,
		LoopedAnimation(textures_, config.name, "flight", kFlightCycleSeconds),
		from, theta, damage);
	// Out of the muzzle, a little ahead of the one firing: fired into a
	// wall (or an enemy) at arm's length, it bursts there
	constexpr double kMuzzle = 0.3;
	if (Fly(projectile, kMuzzle)) {
		projectile.StartTick();
	}
}

void Scene::FlyProjectiles(double delta_time) {
	for (Projectile& projectile : projectiles_) {
		if (projectile.IsFlying()) {
			projectile.StartTick();
			Fly(projectile, projectile.GetConfig().speed * delta_time);
		}
	}
}

bool Scene::Fly(Projectile& projectile, double distance) {
	// Shorter than any body is wide
	constexpr double kStep = 0.05;
	const vector2d direction = projectile.GetDirection();
	const double radius = projectile.GetConfig().radius;
	for (double flown = 0.0; flown < distance; flown += kStep) {
		const vector2d from = projectile.GetPose();
		const vector2d to =
			from + direction * std::min(kStep, distance - flown);
		// Its nose at a wall or a closed door: it bursts short of it, on
		// the near side
		if (map_.IsBlocked(to + direction * radius)) {
			Burst(projectile, from, nullptr);
			return false;
		}
		for (Enemy* enemy : enemy_list_) {
			if (enemy->IsAlive() && enemy->GetHealth() > 0.0 &&
				enemy->GetPose().Distance(to) < enemy->GetRadius() + radius) {
				Burst(projectile, to, enemy);
				return false;
			}
		}
		projectile.MoveTo(to);
	}
	return true;
}

void Scene::Burst(Projectile& projectile, const vector2d& at, Enemy* struck) {
	const ProjectileConfig& config = projectile.GetConfig();
	projectile.Stop();
	bool hurt = struck != nullptr && Wound(*struck, projectile.GetDamage());
	// Its blast, falling off from the burst to its edge, on whoever's body
	// it reaches with nothing in between
	if (config.splash_radius > 0.0) {
		const auto blast = [&](const vector2d& pose,
							   double radius) -> std::optional<double> {
			const double reach = std::max(pose.Distance(at) - radius, 0.0) /
								 config.splash_radius;
			if (reach >= 1.0 || !CastLineOfSight(map_, at, pose).is_hit) {
				return std::nullopt;
			}
			return config.splash_damage.first +
				   (config.splash_damage.second - config.splash_damage.first) *
					   reach;
		};
		for (Enemy* enemy : enemy_list_) {
			if (const auto damage =
					blast(enemy->GetPose(), enemy->GetRadius())) {
				hurt = Wound(*enemy, *damage) || hurt;
			}
		}
		// Caught in their own blast, the player takes half: enough to
		// teach care, not to end a game at a wall
		constexpr double kOwnBlast = 0.5;
		if (player_->IsAlive()) {
			if (const auto damage =
					blast(player_->GetPose(), player_->GetWidth() / 2)) {
				player_->DecreaseHealth(kOwnBlast * *damage);
			}
		}
	}
	if (hurt) {
		player_->NoteHit();
	}
	constexpr double kBurstFrameSeconds = 0.1;
	effects_[next_effect_].Start(
		at, LoopedAnimation::Clip(textures_, config.name, "burst"),
		kBurstFrameSeconds, config.burst_width, config.burst_height,
		std::max(Projectile::kFlightHeight - config.burst_height / 2, 0.0));
	next_effect_ = (next_effect_ + 1) % kEffects;
	if (config.burst_sound) {
		sound_.PlayEffect(burst_channel_, *config.burst_sound);
	}
	MakeNoise(at, config.noise_range);
}

void Scene::AddWallMark(const WallMark& mark) {
	wall_marks_[next_wall_mark_] = mark;
	next_wall_mark_ = (next_wall_mark_ + 1) % kWallMarks;
	wall_mark_count_ = std::min(wall_mark_count_ + 1, kWallMarks);
}

void Scene::DecreaseAliveEnemies() {
	// Counting a kill twice would wrap the count, and the level would never
	// end
	assert(number_of_alive_enemies > 0 && "an enemy killed twice");
	if (number_of_alive_enemies > 0) {
		--number_of_alive_enemies;
	}
}

void Scene::Update(double delta_time) {
	if (number_of_alive_enemies > 0) {
		elapsed_ += delta_time;
	}
	{
		ScopedTimer timer(ProfileSection::UpdateEnemies);
		for (IGameObject* object : objects_) {
			object->Update(delta_time);
		}
	}

	ScopedTimer timer(ProfileSection::UpdatePlayer);
	player_->Update(delta_time);
	FlyProjectiles(delta_time);
	CollectPickups();
	notice_time_ += delta_time;
	HandleUse();
	map_.AdvancePushWalls(PushWall::kDistance * delta_time / kPushSeconds);
	UpdateDoors(delta_time);
}

Scene::Notice Scene::GetNotice() const {
	constexpr double kNoticeSeconds = 2.0;
	return notice_time_ < kNoticeSeconds ? notice_ : Notice::None;
}

void Scene::ShowNotice(Notice notice) {
	notice_ = notice;
	notice_time_ = 0.0;
}

void Scene::SetGoals(bool kill_all, bool kill_targets) {
	kill_all_ = kill_all;
	kill_targets_ = kill_targets;
}

std::size_t Scene::TargetsLeft() const {
	return static_cast<std::size_t>(
		std::ranges::count_if(enemy_list_, [](const Enemy* enemy) {
			return enemy->IsTarget() && enemy->GetHealth() > 0.0;
		}));
}

bool Scene::ObjectivesDone() const {
	return (!kill_all_ || number_of_alive_enemies == 0) &&
		   (!kill_targets_ || TargetsLeft() == 0);
}

bool Scene::IsComplete() const {
	return map_.HasExit() ? completed_ : number_of_alive_enemies == 0;
}

void Scene::UseExit() {
	if (!map_.HasExit()) {
		return;
	}
	if (ObjectivesDone()) {
		completed_ = true;
	}
	else {
		ShowNotice(Notice::ExitLocked);
	}
}

// The player uses what is just ahead: a door (if it has the key) or the
// exit switch
void Scene::HandleUse() {
	if (!player_->IsAlive() || !player_->IsUsing()) {
		return;
	}
	const Position2D& eye = player_->GetPosition();
	const vector2d facing{std::cos(eye.theta), std::sin(eye.theta)};
	for (const double reach : {0.6, 1.2}) {
		const vector2d point = eye.pose + facing * reach;
		const int x = static_cast<int>(std::floor(point.x));
		const int y = static_cast<int>(std::floor(point.y));
		if (map_.IsExit(x, y)) {
			UseExit();
			return;
		}
		if (const PushWall* wall = map_.FindPushWall(x, y)) {
			map_.Push(
				static_cast<std::size_t>(wall - map_.GetPushWalls().data()));
			ShowNotice(Notice::Secret);
			return;
		}
		if (const Door* door = map_.FindDoor(x, y)) {
			if (door->lock == KeyColour::None || player_->HasKey(door->lock)) {
				OpenDoor(
					static_cast<std::size_t>(door - map_.GetDoors().data()));
			}
			else {
				ShowNotice(door->lock == KeyColour::Gold
							   ? Notice::NeedGoldKey
							   : Notice::NeedSilverKey);
			}
			return;
		}
		if (map_.IsWall(x, y)) {
			return;	 // nothing to use through a wall
		}
	}
}

void Scene::OpenDoor(std::size_t door) {
	DoorMotion& motion = doors_[door];
	if (motion.phase == DoorMotion::Phase::Closed) {
		sound_.PlayEffect(door_channel_, SoundEffect::DoorMove);
	}
	if (motion.phase == DoorMotion::Phase::Closed ||
		motion.phase == DoorMotion::Phase::Closing) {
		motion.phase = DoorMotion::Phase::Opening;
	}
}

bool Scene::IsDoorwayOccupied(const Door& door) const {
	// Anyone whose body reaches into the door's cell
	constexpr double kReach = 0.8;
	const vector2d centre{door.x + 0.5, door.y + 0.5};
	const auto near = [&](const vector2d& pose) {
		return std::abs(pose.x - centre.x) < kReach &&
			   std::abs(pose.y - centre.y) < kReach;
	};
	if (player_->IsAlive() && near(player_->GetPose())) {
		return true;
	}
	return std::ranges::any_of(enemy_list_, [&](const Enemy* enemy) {
		return enemy->IsAlive() && near(enemy->GetPose());
	});
}

void Scene::UpdateDoors(double delta_time) {
	const auto doors = map_.GetDoors();
	if (doors.empty()) {
		return;
	}
	// Enemies open the doors they walk up to, if not locked
	constexpr double kEnemyReach = 1.2;
	for (std::size_t i = 0; i < doors.size(); ++i) {
		if (doors[i].lock != KeyColour::None) {
			continue;
		}
		const vector2d centre{doors[i].x + 0.5, doors[i].y + 0.5};
		if (std::ranges::any_of(enemy_list_, [&](const Enemy* enemy) {
				return enemy->IsAlive() &&
					   enemy->GetPose().Distance(centre) < kEnemyReach;
			})) {
			OpenDoor(i);
		}
	}

	const double step = delta_time / kDoorMoveSeconds;
	for (std::size_t i = 0; i < doors.size(); ++i) {
		DoorMotion& motion = doors_[i];
		const double openness = doors[i].openness;
		switch (motion.phase) {
			case DoorMotion::Phase::Closed:
				break;
			case DoorMotion::Phase::Opening:
				map_.SetDoorOpenness(i, openness + step);
				if (openness + step >= 1.0) {
					motion.phase = DoorMotion::Phase::Open;
					motion.open_time = 0.0;
				}
				break;
			case DoorMotion::Phase::Open:
				motion.open_time += delta_time;
				if (motion.open_time >= kDoorOpenSeconds &&
					!IsDoorwayOccupied(doors[i])) {
					motion.phase = DoorMotion::Phase::Closing;
					sound_.PlayEffect(door_channel_, SoundEffect::DoorMove);
				}
				break;
			case DoorMotion::Phase::Closing:
				// Never onto someone: it opens again for them
				if (IsDoorwayOccupied(doors[i])) {
					motion.phase = DoorMotion::Phase::Opening;
					break;
				}
				map_.SetDoorOpenness(i, openness - step);
				if (openness - step <= 0.0) {
					motion.phase = DoorMotion::Phase::Closed;
				}
				break;
		}
	}
}

void Scene::CollectPickups() {
	if (!player_->IsAlive()) {
		return;
	}
	const vector2d position = player_->GetPose();
	for (Pickup* pickup : pickup_list_) {
		// Close enough that the player's body touches the item
		const double reach = (player_->GetWidth() + pickup->GetWidth()) / 2;
		if (!pickup->IsTaken() &&
			pickup->GetPose().Distance(position) <= reach &&
			player_->TryPickUp(pickup->GetEffect(), difficulty_.supplies)) {
			pickup->Take();
		}
	}
}

void Scene::Explore(int x, int y) {
	const int size_x = map_.GetSizeX();
	const int size_y = map_.GetSizeY();
	if (x < 0 || y < 0 || x >= size_x || y >= size_y) {
		return;
	}
	std::uint8_t& flag = explored_[(static_cast<std::size_t>(x) *
									static_cast<std::size_t>(size_y)) +
								   static_cast<std::size_t>(y)];
	if (flag == 0) {
		flag = 1;
		explored_open_cells_ += map_.IsWall(x, y) ? 0 : 1;
	}
}

bool Scene::IsExplored(int x, int y) const {
	const int size_x = map_.GetSizeX();
	const int size_y = map_.GetSizeY();
	return x >= 0 && y >= 0 && x < size_x && y < size_y &&
		   explored_[(static_cast<std::size_t>(x) *
					  static_cast<std::size_t>(size_y)) +
					 static_cast<std::size_t>(y)] != 0;
}

const Map& Scene::GetMap() const {
	return map_;
}

Map& Scene::GetMap() {
	return map_;
}

const Player& Scene::GetPlayer() const {
	return *player_;
}

Player& Scene::GetPlayer() {
	return *player_;
}

size_t Scene::GetNumberOfAliveEnemies() const {
	return number_of_alive_enemies;
}

bool Scene::IsQuiet() const {
	return std::ranges::all_of(enemy_list_, &Enemy::IsCalm);
}

void Scene::RestoreKilled(std::size_t index) {
	Enemy* enemy = enemy_list_[index];
	if (!enemy->IsAlive()) {
		return;
	}
	enemy->RestoreDead();
	navigation_.ResetPath(enemy->GetId());
	DecreaseAliveEnemies();
}

LevelStats Scene::GetStats() const {
	// The level's own supplies, not what enemies drop
	const auto supplies = static_cast<std::size_t>(std::ranges::count_if(
		pickup_list_, [](const Pickup* pickup) { return !pickup->IsDrop(); }));
	const auto taken = static_cast<std::size_t>(
		std::ranges::count_if(pickup_list_, [](const Pickup* pickup) {
			return !pickup->IsDrop() && pickup->IsTaken();
		}));
	const auto secrets = map_.GetPushWalls();
	return {.kills = enemy_list_.size() - number_of_alive_enemies,
			.enemies = enemy_list_.size(),
			.pickups_taken = taken,
			.pickups = supplies,
			.secrets_found = static_cast<std::size_t>(
				std::ranges::count_if(secrets, &PushWall::pushed)),
			.secrets = secrets.size(),
			.explored_percent =
				open_cells_ == 0 ? 0
								 : static_cast<int>(100 * explored_open_cells_ /
													open_cells_),
			.seconds = elapsed_};
}

}  // namespace wolfenstein
