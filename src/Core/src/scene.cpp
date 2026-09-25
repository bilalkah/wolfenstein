#include "Core/scene.h"
#include "Profiler/profiler.h"
namespace wolfenstein {

namespace {

// Size of the level arena: the pools' storage and bookkeeping and the two
// object lists, each with room for alignment padding. Exceeding it throws.
std::size_t LevelArenaBytes(SceneCapacity capacity) {
	constexpr std::size_t kSlack = 256;
	constexpr std::size_t kBookkeeping =
		sizeof(std::uint32_t) * 2 + sizeof(std::uint8_t);
	const std::size_t enemies = capacity.enemies;
	const std::size_t objects = capacity.enemies + capacity.dynamic_objects;
	return enemies * (sizeof(Enemy) + alignof(Enemy) + kBookkeeping) +
		   capacity.dynamic_objects *
			   (sizeof(DynamicObject) + alignof(DynamicObject) + kBookkeeping) +
		   objects * sizeof(IGameObject*) + enemies * sizeof(Enemy*) +
		   8 * kSlack;
}

}  // namespace

Scene::Scene(SceneCapacity capacity)
	: arena_(LevelArenaBytes(capacity)),
	  enemies_(capacity.enemies, &arena_),
	  dynamic_objects_(capacity.dynamic_objects, &arena_),
	  objects_(&arena_),
	  enemy_list_(&arena_) {
	objects_.reserve(capacity.enemies + capacity.dynamic_objects);
	enemy_list_.reserve(capacity.enemies);
}

std::expected<memory::Handle<Enemy>, memory::PoolError> Scene::AddEnemy(
	const std::string& type, const CharacterConfig& config) {
	auto handle = enemies_.Create(type, config);
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

void Scene::SetMap(std::shared_ptr<Map> map) {
	this->map = map;
}

void Scene::SetPlayer(std::shared_ptr<Player>& player) {
	this->player = player;
}

void Scene::SetNextScene(const std::string next_scene) {
	next_scene_str = next_scene;
}

void Scene::DecreaseAliveEnemies() {
	number_of_alive_enemies--;
}

void Scene::Update(double delta_time) {
	{
		ScopedTimer timer(ProfileSection::UpdateEnemies);
		for (IGameObject* object : objects_) {
			object->Update(delta_time);
		}
	}

	ScopedTimer timer(ProfileSection::UpdatePlayer);
	player->Update(delta_time);
}

const Map& Scene::GetMap() const {
	return *map;
}

Map& Scene::GetMap() {
	return *map;
}

const Player& Scene::GetPlayer() const {
	return *player;
}

Player& Scene::GetPlayer() {
	return *player;
}

size_t Scene::GetNumberOfAliveEnemies() const {
	return number_of_alive_enemies;
}

const std::string& Scene::GetNextScene() const {
	return next_scene_str;
}

}  // namespace wolfenstein
