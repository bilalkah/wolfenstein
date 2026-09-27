#include "Core/scene.h"
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
								capacity.pickups + Scene::kEffects;
	return enemies * (sizeof(Enemy) + alignof(Enemy) + kBookkeeping) +
		   capacity.dynamic_objects *
			   (sizeof(DynamicObject) + alignof(DynamicObject) + kBookkeeping) +
		   pickups * (sizeof(Pickup) + alignof(Pickup) + kBookkeeping) +
		   objects * sizeof(IGameObject*) + enemies * sizeof(Enemy*) +
		   pickups * sizeof(Pickup*) +
		   std::size_t{map.GetSizeX()} * map.GetSizeY() +  // explored flags
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
	  doors_(map.GetDoors().size(), DoorMotion{}, &arena_) {
	map_.ReservePushWalls(capacity.secrets);
	objects_.reserve(capacity.enemies + capacity.dynamic_objects +
					 capacity.pickups + kEffects);
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
	navigation_.Build();
}

void Scene::ShowImpact(Impact impact, const vector2d& pose) {
	// Drawn 0.4 of a wall wide and 0.8 high: the puff is where shots fly in
	// the art, half a wall up
	constexpr double kFrameSeconds = 0.06;
	constexpr double kWidth = 0.4;
	constexpr double kHeight = 0.8;
	effects_[next_effect_].Start(
		pose, impact == Impact::Blood ? blood_frames_ : dust_frames_,
		kFrameSeconds, kWidth, kHeight);
	next_effect_ = (next_effect_ + 1) % kEffects;
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
	const auto taken = static_cast<std::size_t>(
		std::ranges::count_if(pickup_list_, &Pickup::IsTaken));
	const auto secrets = map_.GetPushWalls();
	return {.kills = enemy_list_.size() - number_of_alive_enemies,
			.enemies = enemy_list_.size(),
			.pickups_taken = taken,
			.pickups = pickup_list_.size(),
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
