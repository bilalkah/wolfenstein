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
#include "GameObjects/effect.h"
#include "GameObjects/game_object.h"
#include "GameObjects/pickup.h"
#include "GameObjects/projectile.h"
#include "NavigationManager/navigation_manager.h"
#include <array>
#include <cstdint>
#include <expected>
#include <memory_resource>
#include <span>
#include <string>

namespace wolfenstein {

// How hard a game is: multipliers on the damage enemies deal, the health
// they start with and what pickups give
struct Difficulty
{
	double enemy_damage = 1.0;
	double enemy_health = 1.0;
	double supplies = 1.0;
};

// How the player did in a level: shown on the HUD and when it is cleared
struct LevelStats
{
	std::size_t kills = 0;
	std::size_t enemies = 0;
	std::size_t pickups_taken = 0;
	std::size_t pickups = 0;
	std::size_t secrets_found = 0;
	std::size_t secrets = 0;
	int explored_percent = 0;  // of the cells the player can stand in
	double seconds = 0.0;	   // until the last enemy fell
};

// How many level objects a scene must hold; known when the level is loaded
struct SceneCapacity
{
	std::uint32_t enemies = 0;
	std::uint32_t dynamic_objects = 0;
	std::uint32_t pickups = 0;
	std::uint32_t secrets = 0;
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
					 double width, double height, double radius = 0.0);
	std::expected<memory::Handle<Pickup>, memory::PoolError> AddPickup(
		const vector2d& pose, int texture_id, double width, double height,
		const PickupEffect& effect);
	// Applies to enemies added after it and to every pickup taken
	void SetDifficulty(const Difficulty& difficulty) {
		difficulty_ = difficulty;
	}
	const Difficulty& GetDifficulty() const { return difficulty_; }
	// Borrows the player for the scene's life and lets it act in this scene
	void SetPlayer(Player& player);
	// Builds what depends on the finished level (the navigation grid): call
	// once every object is in place
	void FinishLoading();
	void DecreaseAliveEnemies();
	// Starts opening the door with this index in the map (if not open)
	void OpenDoor(std::size_t door);
	// A message for the player, for a moment after what caused it
	enum class Notice : std::uint8_t {
		None,
		NeedGoldKey,  // tried a gold-locked door without the key
		NeedSilverKey,
		ExitLocked,	 // used the exit before the objectives were done
		Secret,		 // found a secret
	};
	Notice GetNotice() const;

	// What the level asks before its exit opens (its objectives' types)
	void SetGoals(bool kill_all, bool kill_targets);
	std::size_t TargetsLeft() const;
	bool ObjectivesDone() const;
	// Whether the level is over: its exit used with the objectives done, or
	// for a level without an exit, every enemy dead
	bool IsComplete() const;
	// The player uses the exit switch
	void UseExit();
	// How long a secret takes to slide all the way back
	static constexpr double kPushSeconds = 1.0;

	void Update(double delta_time);

	// Where a shot lands: a puff of blood on an enemy, of dust on a wall
	enum class Impact : std::uint8_t { Blood, Dust };
	// Puffs showing at once, at most: a new one takes the oldest's place
	static constexpr std::size_t kEffects = 12;
	// Bullet marks the walls keep: a new one takes the oldest's place
	static constexpr std::size_t kWallMarks = 32;
	// A mark where a shot struck the face of a wall cell: across the face
	// (as its texture runs, 0 to 1) and down it (0 at the top)
	struct WallMark
	{
		int x = 0;
		int y = 0;
		std::uint8_t face = 0;	// as HitFace() tells it
		float across = 0.0F;
		float down = 0.0F;
	};
	// A puff at `pose`, `elevation` above where shots fly level (half a wall
	// up)
	// A noise at `pose` (a gunshot): it spreads through open floor, round
	// corners but not through walls or closed doors, as far as `range`
	// cells, and alerts every living enemy it reaches. Allocates nothing.
	void MakeNoise(const vector2d& pose, int range);
	// A puff at `pose`, centred `height` above the floor (where the shot
	// struck), `scale` times its usual size (a headshot's is bigger)
	void ShowImpact(Impact impact, const vector2d& pose, double height,
					double scale = 1.0);
	void AddWallMark(const WallMark& mark);
	std::span<const WallMark> GetWallMarks() const {
		return std::span(wall_marks_).first(wall_mark_count_);
	}

	// Projectiles in flight at once, at most: a new one takes the oldest's
	// place
	static constexpr std::size_t kProjectiles = 16;
	// Fires a projectile from `from` along `theta` (the player's rocket or
	// bolt), doing `damage` to what it strikes. It bursts on the first wall,
	// closed door or living enemy in its way, straight away if that is at
	// the muzzle; its blast, if it has one, hurts whoever is in reach and in
	// sight of the burst, the player too. `config` outlives the level.
	void Launch(const ProjectileConfig& config, const vector2d& from,
				double theta, double damage);
	std::span<const Projectile> GetProjectiles() const { return projectiles_; }
	// Hurts a living enemy: it feels it, cries out to those near it, and a
	// killing blow is counted, once. False, and nothing done, for one
	// already down.
	bool Wound(Enemy& enemy, double damage);

	// Every level object, in update and draw order, the effects and
	// projectiles last; an object's ObjectId is its index here
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
	// Whether no living enemy is engaged with the player
	bool IsQuiet() const;
	// Putting back what a saved game recorded: the level's enemy `index`
	// (in level file order) lying dead, and the level's clock
	void RestoreKilled(std::size_t index);
	void RestoreSeconds(double seconds) { elapsed_ = seconds; }
	const memory::MonotonicArena& LevelMemory() const { return arena_; }

	// How long a door takes to open or close, and stays open
	static constexpr double kDoorMoveSeconds = 0.5;
	static constexpr double kDoorOpenSeconds = 4.0;

  private:
	// The player takes every pickup it stands on and has a use for
	void CollectPickups();
	void HandleUse();
	void ShowNotice(Notice notice);
	// Opens doors the player uses or an enemy reaches, and moves every door
	// on: open doors close again once their doorway is clear
	void UpdateDoors(double delta_time);
	// Whether a living character stands in or at the door's cell
	bool IsDoorwayOccupied(const Door& door) const;
	// Moves each projectile in flight on by its speed
	void FlyProjectiles(double delta_time);
	// Moves a projectile `distance` on, a short step at a time so it passes
	// through no corner or body; false if it burst on the way
	bool Fly(Projectile& projectile, double distance);
	// A projectile bursts at `at`, on `struck` if it hit an enemy
	void Burst(Projectile& projectile, const vector2d& at, Enemy* struck);

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
	// For spreading a noise: how far it has come to each cell (kUnheard
	// where it has not), and the cells still to spread from
	static constexpr std::uint16_t kUnheard = 0xFFFF;
	std::pmr::vector<std::uint16_t> noise_distance_;
	std::pmr::vector<std::uint32_t> noise_queue_;
	// Cells a character can stand in (not walls), and how many are explored
	std::size_t open_cells_ = 0;
	std::size_t explored_open_cells_ = 0;
	// Simulated time in the level, until it is cleared
	double elapsed_ = 0.0;
	Difficulty difficulty_;
	// One per door of the map, in its order
	std::pmr::vector<DoorMotion> doors_;
	Notice notice_ = Notice::None;
	double notice_time_ = 1e9;	// since it was shown
	bool kill_all_ = false;
	bool kill_targets_ = false;
	bool completed_ = false;
	Player* player_ = nullptr;
	SoundChannel door_channel_;	 // doors sliding
	// Kept for the level's life, joining the objects when it is loaded
	std::array<Effect, kEffects> effects_{};
	std::size_t next_effect_ = 0;
	std::span<const std::uint16_t> blood_frames_;
	std::span<const std::uint16_t> dust_frames_;
	std::array<WallMark, kWallMarks> wall_marks_{};
	std::size_t wall_mark_count_ = 0;
	std::size_t next_wall_mark_ = 0;
	// Kept for the level's life like the effects
	std::array<Projectile, kProjectiles> projectiles_{};
	std::size_t next_projectile_ = 0;
	SoundChannel burst_channel_;
	NavigationManager navigation_{*this, &arena_};
	size_t number_of_alive_enemies{};
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_SCENE_H_