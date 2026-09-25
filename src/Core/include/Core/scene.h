/**
 * @file scene.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CORE_INCLUDE_CORE_SCENE_H_
#define CORE_INCLUDE_CORE_SCENE_H_

#include <memory>
#include <vector>

#include "Allocators/monotonic_arena.h"
#include "Allocators/object_pool.h"
#include "Characters/enemy.h"
#include "Characters/player.h"
#include "GameMap/map.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/game_object.h"
#include "NavigationManager/navigation_manager.h"
#include <cstdint>
#include <expected>
#include <memory_resource>
#include <span>
#include <string>

namespace wolfenstein {

// How many level objects a scene must hold; known when the level is loaded
struct SceneCapacity
{
	std::uint32_t enemies = 0;
	std::uint32_t dynamic_objects = 0;
};

// Owns one level. Its objects live in fixed-capacity pools whose storage,
// like the lists that order them, comes from a per-level arena: objects are
// laid out contiguously, creating them needs no per-object heap allocation,
// and tearing the level down releases the whole block at once. It also owns
// the level's systems (navigation), which borrow it.
//
// A scene has a single owner (the game); renderers, the camera, enemies and
// the player borrow it for its lifetime. It borrows the player, which
// outlives levels. Pinned: its objects and systems point back to it.
class Scene
{
  public:
	// Borrows the textures its objects' animations play from and the sound
	// they play; both outlive every level. The map and the capacity size the
	// level's arena, which also holds the navigation data.
	Scene(const TextureManager& textures, SoundManager& sound, Map map,
		  SceneCapacity capacity = {});
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;

	std::expected<memory::Handle<Enemy>, memory::PoolError> AddEnemy(
		const EnemyConfig& config, const Position2D& position);
	std::expected<memory::Handle<DynamicObject>, memory::PoolError>
	AddDynamicObject(const vector2d& pose, const LoopedAnimation& animation,
					 double width, double height);
	// Borrows the player for the scene's life and lets it act in this scene
	void SetPlayer(Player& player);
	// Builds what depends on the finished level (the navigation grid): call
	// once every object is in place
	void FinishLoading();
	void SetNextScene(const std::string next_scene);
	void DecreaseAliveEnemies();

	void Update(double delta_time);

	// Every level object, in update and draw order; an object's ObjectId is
	// its index here
	std::span<IGameObject* const> GetObjects() const { return objects_; }
	std::span<Enemy* const> GetEnemies() const { return enemy_list_; }

	const Map& GetMap() const;
	Map& GetMap();
	const Player& GetPlayer() const;
	Player& GetPlayer();
	const TextureManager& Textures() const { return textures_; }
	SoundManager& Sound() { return sound_; }
	NavigationManager& GetNavigation() { return navigation_; }
	const NavigationManager& GetNavigation() const { return navigation_; }

	size_t GetNumberOfAliveEnemies() const;
	const std::string& GetNextScene() const;
	const memory::MonotonicArena& LevelMemory() const { return arena_; }

  private:
	const TextureManager& textures_;
	SoundManager& sound_;
	Map map_;
	// Declared first of what the scene owns, so it is destroyed last, after
	// everything living in it
	memory::MonotonicArena arena_;
	memory::ObjectPool<Enemy> enemies_;
	memory::ObjectPool<DynamicObject> dynamic_objects_;
	std::pmr::vector<IGameObject*> objects_;
	std::pmr::vector<Enemy*> enemy_list_;
	Player* player_ = nullptr;
	NavigationManager navigation_{*this, &arena_};
	size_t number_of_alive_enemies{};
	std::string next_scene_str;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_H_