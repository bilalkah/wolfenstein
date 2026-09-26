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
#include "GameObjects/pickup.h"
#include "NavigationManager/navigation_manager.h"
#include <cstdint>
#include <expected>
#include <memory_resource>
#include <span>
#include <string>

namespace wolfenstein {

// How the player did in a level: shown on the HUD and when it is cleared
struct LevelStats
{
	std::size_t kills = 0;
	std::size_t enemies = 0;
	std::size_t pickups_taken = 0;
	std::size_t pickups = 0;
	int explored_percent = 0;  // of the cells the player can stand in
	double seconds = 0.0;	   // until the last enemy fell
};

// How many level objects a scene must hold; known when the level is loaded
struct SceneCapacity
{
	std::uint32_t enemies = 0;
	std::uint32_t dynamic_objects = 0;
	std::uint32_t pickups = 0;
};

// Owns one level. Its objects live in fixed-capacity pools whose storage,
// like the lists that order them, the map and the navigation data, comes
// from a level arena it borrows: the World sizes one arena for its largest
// level at startup and resets it between levels, so building a level never
// touches the heap. The scene also owns the level's systems (navigation),
// which borrow it.
//
// A scene has a single owner (the game); renderers, the camera, enemies and
// the player borrow it for its lifetime. It borrows the player, which
// outlives levels. Pinned: its objects and systems point back to it.
class Scene
{
  public:
	// Bytes of arena a level with this map and capacity takes
	static std::size_t MemoryFor(const Map& map, SceneCapacity capacity);

	// Borrows the textures its objects' animations play from, the sound they
	// play and the arena everything level-sized comes from (at least
	// MemoryFor(map, capacity) bytes free); all outlive the scene. The map
	// is copied into the arena.
	Scene(const TextureManager& textures, SoundManager& sound, const Map& map,
		  SceneCapacity capacity, memory::MonotonicArena& arena);
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;
	~Scene() = default;

	std::expected<memory::Handle<Enemy>, memory::PoolError> AddEnemy(
		const EnemyConfig& config, const Position2D& position);
	std::expected<memory::Handle<DynamicObject>, memory::PoolError>
	AddDynamicObject(const vector2d& pose, const LoopedAnimation& animation,
					 double width, double height);
	std::expected<memory::Handle<Pickup>, memory::PoolError> AddPickup(
		const vector2d& pose, int texture_id, double width, double height,
		const PickupEffect& effect);
	// Borrows the player for the scene's life and lets it act in this scene
	void SetPlayer(Player& player);
	// Builds what depends on the finished level (the navigation grid): call
	// once every object is in place
	void FinishLoading();
	void DecreaseAliveEnemies();
	// Starts opening the door with this index in the map (if not open)
	void OpenDoor(std::size_t door);

	void Update(double delta_time);

	// Every level object, in update and draw order; an object's ObjectId is
	// its index here
	std::span<IGameObject* const> GetObjects() const { return objects_; }
	std::span<Enemy* const> GetEnemies() const { return enemy_list_; }
	std::span<Pickup* const> GetPickups() const { return pickup_list_; }

	const Map& GetMap() const;
	Map& GetMap();
	const Player& GetPlayer() const;
	Player& GetPlayer();
	const TextureManager& Textures() const { return textures_; }
	SoundManager& Sound() { return sound_; }
	NavigationManager& GetNavigation() { return navigation_; }
	const NavigationManager& GetNavigation() const { return navigation_; }

	// What the player has seen of the level, cell by cell: the map shows
	// only these. Cells outside the map are ignored.
	void Explore(int x, int y);
	bool IsExplored(int x, int y) const;

	size_t GetNumberOfAliveEnemies() const;
	LevelStats GetStats() const;
	const memory::MonotonicArena& LevelMemory() const { return arena_; }

	// How long a door takes to open or close, and stays open
	static constexpr double kDoorMoveSeconds = 0.5;
	static constexpr double kDoorOpenSeconds = 4.0;

  private:
	// The player takes every pickup it stands on and has a use for
	void CollectPickups();
	// Opens doors the player uses or an enemy reaches, and moves every door
	// on: open doors close again once their doorway is clear
	void UpdateDoors(double delta_time);
	// Whether a living character stands in or at the door's cell
	bool IsDoorwayOccupied(const Door& door) const;

	struct DoorMotion
	{
		enum class Phase : std::uint8_t { Closed, Opening, Open, Closing };
		Phase phase = Phase::Closed;
		double open_time = 0.0;	 // how long it has stood open
	};

	const TextureManager& textures_;
	SoundManager& sound_;
	// Borrowed; everything below that the scene owns lives in it
	memory::MonotonicArena& arena_;
	// The level's map, with its cells in the arena
	Map map_;
	memory::ObjectPool<Enemy> enemies_;
	memory::ObjectPool<DynamicObject> dynamic_objects_;
	memory::ObjectPool<Pickup> pickups_;
	std::pmr::vector<IGameObject*> objects_;
	std::pmr::vector<Enemy*> enemy_list_;
	std::pmr::vector<Pickup*> pickup_list_;
	// One flag per map cell, row by row
	std::pmr::vector<std::uint8_t> explored_;
	// Cells a character can stand in (not walls), and how many are explored
	std::size_t open_cells_ = 0;
	std::size_t explored_open_cells_ = 0;
	// Simulated time in the level, until it is cleared
	double elapsed_ = 0.0;
	// One per door of the map, in its order
	std::pmr::vector<DoorMotion> doors_;
	Player* player_ = nullptr;
	NavigationManager navigation_{*this, &arena_};
	size_t number_of_alive_enemies{};
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_H_