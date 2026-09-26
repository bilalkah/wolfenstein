#include "Core/scene.h"
#include "Profiler/profiler.h"
#include <cassert>
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
	const std::size_t objects =
		capacity.enemies + capacity.dynamic_objects + capacity.pickups;
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
	return LevelArenaBytes(map, capacity);
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
	  explored_(std::size_t{map.GetSizeX()} * map.GetSizeY(), 0, &arena_) {
	objects_.reserve(capacity.enemies + capacity.dynamic_objects +
					 capacity.pickups);
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
						double width, double height) {
	auto handle = dynamic_objects_.Create(pose, animation, width, height);
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
	navigation_.Build();
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
	{
		ScopedTimer timer(ProfileSection::UpdateEnemies);
		for (IGameObject* object : objects_) {
			object->Update(delta_time);
		}
	}

	ScopedTimer timer(ProfileSection::UpdatePlayer);
	player_->Update(delta_time);
	CollectPickups();
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
			player_->TryPickUp(pickup->GetEffect())) {
			pickup->Take();
		}
	}
}

void Scene::Explore(int x, int y) {
	const int size_x = map_.GetSizeX();
	const int size_y = map_.GetSizeY();
	if (x >= 0 && y >= 0 && x < size_x && y < size_y) {
		explored_[(static_cast<std::size_t>(x) *
				   static_cast<std::size_t>(size_y)) +
				  static_cast<std::size_t>(y)] = 1;
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

}  // namespace wolfenstein
