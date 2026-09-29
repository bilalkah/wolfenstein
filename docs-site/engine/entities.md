# Game objects and the scene

## Purpose

A level is more than its map: enemies, lamps, pickups lying about, puffs
where shots landed, rockets in flight, marks on the walls, pages of intel.
`Scene` owns all of them for the level's life and runs the level's own
systems: doors, noise, sounds from places, notices, statistics, the
explored cells. `IGameObject` is the one interface every thing in the level
shares, so the scene can update them in one loop and the camera can draw
them in one loop.

Code: `src/GameObjects/`, `src/Core/include/Core/scene.h`,
`src/Core/src/scene.cpp`.

## Concepts

### An interface, not an entity-component system

Engines organise game objects in broadly two ways. In an **object
hierarchy**, each kind of thing is a class implementing a common interface
(`Update`, `GetPose`, ...), and the world keeps a list of them. In an
**entity-component system**, a thing is an id, its data lives in
per-component arrays, and systems loop over the arrays. This engine uses
the first, deliberately small: one abstract base class, a handful of
kinds, and plain arrays elsewhere indexed by the object's id where a
system needs per-object data (the camera's views, the enemies' routes).

### Stable identities without allocation

An object's `ObjectId` is its **index in the scene's object list**. Objects
are never removed during a level: a taken pickup is hidden, a dead enemy
lies where it fell, a burst projectile waits to be launched again. So an
index stays valid for the whole level, and "per-object" data can be a
`std::vector` indexed by it instead of a hash map keyed by a string (which
is what the 2024 code used: uuid strings, `1819c53`).

```cpp title="src/GameObjects/include/GameObjects/object_id.h"
// Identifies an object within its scene: its index in the scene's object
// list. Per-object data elsewhere (camera views, enemy routes) is then a
// plain array indexed by it, where string ids needed a hash map lookup (and
// a string copy) every time.
enum class ObjectId : std::uint32_t {
    None = std::numeric_limits<std::uint32_t>::max()
};

constexpr std::size_t ToIndex(ObjectId id) noexcept {
    return std::to_underlying(id);
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/GameObjects/include/GameObjects/object_id.h#L16-L26){ .excerpt-source }

## How it is implemented here

### The interface

`IGameObject` (in `game_object.h`) asks every object for its update, its
pose (and its pose between ticks, for drawing), its type, whether it is
visible, how solid it is, how it looks from a given viewer (picture, width,
mirrored), its size and its elevation off the floor. Defaults cover the
common case: most objects look the same from everywhere, are not solid and
stand on the floor.

| Kind | Class | Lives in | Notes |
| --- | --- | --- | --- |
| Enemy | `Enemy` | `ObjectPool<Enemy>` | Solid while alive; 8 views; its own state machine and weapon |
| Lamp | `DynamicObject` | `ObjectPool<DynamicObject>` | An animated light with a solid base |
| Pickup | `Pickup` | `ObjectPool<Pickup>` | Hidden once taken; enemies' drops are hidden until they die |
| Puff | `Effect` | `std::array<Effect, 12>` | Blood or dust where a shot landed; the oldest is reused |
| Projectile | `Projectile` | `std::array<Projectile, 16>` | Rockets and plasma bolts; the oldest is reused |
| The player | `Player` | `World::player_` | An `IGameObject` too, but not in the level's list: it outlives levels |

Wall marks (32, a ring) and pages of intel (up to 8) are not objects: they
are drawn as [decals](renderer.md) on wall faces.

### Storage

The scene builds everything in the level's arena, sized for exactly this
level:

```cpp title="src/Core/src/scene.cpp"
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
      doors_(map.GetDoors().size(), DoorMotion{}, &arena_) {
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
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene.cpp#L48-L78){ .excerpt-source }

Every container here is a `std::pmr` container handed the arena, and each
is reserved to its final size before anything is added, so filling the
level allocates nothing from the heap and never grows a container.

Adding an object gives it its id and puts it in the lists:

```cpp title="src/Core/src/scene.cpp"
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
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene.cpp#L80-L91){ .excerpt-source }

When the loader has added everything, `FinishLoading` appends the fixed
effects and projectiles to the object list (last, so they draw and update
after the rest) and builds the pathfinding grid:

```cpp title="src/Core/src/scene.cpp"
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
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene.cpp#L124-L136){ .excerpt-source }

### Rings for short-lived things

A puff, a projectile or a bullet mark lasts a moment, and there is a
fixed number of each. The scene reuses them round-robin: the next one
takes the oldest one's place.

```cpp title="src/Core/src/scene.cpp"
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
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene.cpp#L190-L201){ .excerpt-source }

### Pickups and drops

A `Pickup` carries a `PickupEffect`: health, ammunition boxes (a box is as
many rounds as the weapon's `box_rounds`; a dropped clip is half a box),
keys and weapons as bit sets. `Scene::CollectPickups` offers every pickup
the player touches to `Player::TryPickUp`, which takes it only if the
player has a use for it (a medkit at full health stays lying). Taking it
only hides it.

What an enemy may drop is created **at level load** too, hidden, and
handed to the enemy: `SceneLoader::Populate` rolls, from the game's seed,
which drops each enemy actually carries. Every possible drop exists, so
the object list and the saved game's bit per pickup are the same whatever
was rolled. When the enemy dies, its drops are placed where it fell.

### The scene's own systems

Besides holding objects, the scene runs:

- **Doors**: opening on use or when an enemy walks up to an unlocked one,
  staying open four seconds, never closing on someone.
- **Noise**: a breadth-first flood fill through open cells from a gunshot,
  alerting every enemy it reaches (see [Enemy AI](ai.md)).
- **Sounds from places**: `PlaySoundAt` decides whether a sound is muffled
  (no line of sight to the player) and hands it to the spatial mixer.
- **Use**: what the player's use key touches in front of them: the exit, a
  secret wall, a door (locked or not), a page of intel.
- **Notices**: "You need the gold key", "You found a secret".
- **Statistics**: kills, supplies taken, secrets, intel, explored share,
  time.
- **Objectives**: kill everything, or kill the marked targets, before the
  exit opens.

## Design decisions and trade-offs

- **Nothing is removed during a level.** Ids stay indices, lists never
  shift, and there is no deferred-deletion machinery. The cost is that
  loops over objects skip the hidden and the dead.
- **Virtual functions.** One virtual call per object per update and draw,
  about a hundred objects in the largest levels (enemies, lamps, pickups,
  every possible drop, the puffs and the projectiles): negligible, and
  simpler than a component system.
- **Pools per concrete type, plus a list of base pointers.** Enemies are
  contiguous in memory (good for the enemy loops), and the object list
  gives the uniform view.

## Pitfalls

- `dynamic_cast` appears once, in `Camera2D::Calculate`, to reach an
  `Enemy` from an `IGameObject` for the crosshair test; the object's type
  is checked first. A new object kind that should be targeted needs the
  same care.
- Rings of fixed size mean a burst of more than 12 impacts (a shotgun blast
  can make several) recycles puffs still showing. Visually harmless.

## Possible improvements

- Keep per-kind update loops (enemies, then lamps, then pickups) instead of
  one virtual loop, if profiling ever shows `UpdateEnemies` dominated by
  dispatch; it does not today.
