# Levels and content data

## Purpose

Almost everything that makes the game *this* game is data, not code: the
enemy types and their tactics, the weapons and their damage, the pickups,
the difficulties, the campaign's chapters and story, and fifteen levels
with their maps, enemies, lamps, pickups, secrets, objectives, briefings
and intel. This page covers the files, how they are read and checked, and
the script that builds the levels.

Code: `src/Core/src/level_data.cpp`, `src/Core/src/scene_loader.cpp`,
`src/Core/include/Core/story.h`, `scripts/make_levels.py`; data in
`assets/levels/`.

## Concepts

### Data-driven design

Putting content in data files lets it change without recompiling, lets
tools generate it, and makes the code say only *how* things behave, not
*what* there is. The price is validation: a typo in data is found when the
data is read, so reading must check everything and say exactly what is
wrong.

### Procedural levels from hand-written layouts

The levels are not hand-drawn cell by cell, nor random. Each is written as
a short list of rooms (rectangles), corridors between them, pillars,
locks, where the player starts and what each room holds. A script turns
that into the grid, hangs doors, carves hidden rooms, places enemies,
lamps and pickups by rules, and checks the result. The layout stays
readable; the tedious part is automated and repeatable.

## How it is implemented here

### The files

| File | Contents |
| --- | --- |
| `levels/config.json` | `player_config`, `difficulties`, `weapons`, `hit_zones`, `config_enemy` (every enemy type), `pickups`, `config_dynamic` (lamps), `campaign` (the opening, three chapters with their levels, the ending), `benchmark_level`, `menu_music` |
| `levels/<level>.json` | `name`, `briefing`, `debrief`, `music`, `map`, `player`, `enemies`, `dynamicObjects` (lamps), `pickups`, `objectives`, `secrets`, `intel` |
| `maps/<level>.txt` | The grid (see [The map](map.md)) |

A level file's entries look like this:

```json title="assets/levels/level1.json (excerpts)"
{"type": "soldier", "position": {"x": 3.5, "y": 14.5, "theta": -1.25}, "patrol_radius": 3.0}
{"type": "ammo_box", "position": {"x": 1.5, "y": 6.5}}
{"type": "kill_all", "text": "Clear the checkpoint of its guards"}
{"x": 17, "y": 25, "dx": 0, "dy": 1}
```

(an enemy, a pickup, an objective and a secret wall).

### Reading: a tree for the config, a stream for levels

`config.json` is parsed with nlohmann/json's tree API and converted into
typed structs (`GameConfig`, `EnemyConfig`, `WeaponConfig`, ...); the tree
is thrown away afterwards. Every field is read with `at()` (which throws
on a missing key, caught and turned into an error message) rather than
`operator[]` (which would silently insert a null).

Level files are read with a **SAX reader** (`LevelReader`): the parser
calls back for each key, value and bracket, and the reader writes straight
into `LevelData`, never building a JSON tree. A small stack of frames
tracks where in the document the parser is (root, an enemy, a position,
...) and which required fields each object has seen; unknown keys are
skipped. See [Streaming JSON with SAX](../techniques/sax-parsing.md).

### Preparing, once

`SceneLoader::Open` reads the config and every level at startup (see
[Data flow](../architecture/data-flow.md)). Preparing a level checks what
parsing cannot: every enemy and pickup type exists in the config, every
secret is a wall with open floor behind it, every page of intel is on a
wall's face with an open cell in front, a level has at most eight pages;
and it computes the level's `SceneCapacity` and arena size, including room
for every drop its enemies might carry.

### Populating, per level

`SceneLoader::Populate` fills a fresh `Scene`: enemies (with their targets
and patrol radii), lamps, pickups, then every possible **drop**, then the
secrets and the intel, the objectives, the player's position, and the
navigation grid. Which drops an enemy actually carries is rolled from the
game's seed:

```cpp title="src/Core/src/scene_loader.cpp"
    // What each enemy may drop, after the level's own pickups (a saved game
    // counts them in this order), hidden: everything it may drop is made,
    // so the pickups are the same whatever it turns out to carry. Which it
    // does carry is the game's roll (its seed), the same each time the
    // level is loaded, and more likely the more supplies the difficulty
    // gives.
    const auto enemies = scene.GetEnemies();
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        const auto& drops =
            config_.enemies.find(enemies[i]->GetBotName())->second.drops;
        for (std::size_t j = 0; j < drops.size(); ++j) {
            const PickupConfig& pickup =
                config_.pickups.find(drops[j].pickup)->second;
            if (!scene.AddPickup(enemies[i]->GetPose(),
                                 scene.Textures().GetTextureId(pickup.texture),
                                 pickup.width, pickup.height, pickup.effect)) {
                return std::unexpected("more pickups than the scene can hold");
            }
            Pickup* made = scene.GetPickups().back();
            made->MakeDrop();
            const double chance =
                std::min(drops[j].chance * scene.GetDifficulty().supplies, 1.0);
            if (DropRoll(seed, level.data.map, i, j) < chance) {
                enemies[i]->AddDrop(*made);
            }
        }
    }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene_loader.cpp#L204-L230){ .excerpt-source }

`DropRoll` hashes the level's map file name (FNV-1a), mixes in the seed, the enemy's
index and the drop's index, and finishes with the SplitMix64 mixer, giving
a number in \([0, 1)\). The same seed gives the same drops every time the
level is loaded, which is what lets a saved game come back with the same
drops.

### The level generator

`scripts/make_levels.py` holds every level as a `Level` of `Room`s:

```python title="scripts/make_levels.py"
@dataclass
class Room:
    name: str
    x0: int  # floor rectangle, inclusive
    y0: int
    x1: int
    y1: int
    wall: int = CONCRETE
    # (count, type) of enemies in it, or (count, type, "target") for ones
    # a kill_targets objective asks for; and how many lights
    enemies: list = field(default_factory=list)
    lights: int = 0
    light: str = "green_light"
    # (count, type) of pickups lying in it
    pickups: list = field(default_factory=list)
    # Pages of intel pinned to its walls, (title, text): read by walking up
    # to one and looking at it
    intel: list = field(default_factory=list)

    def cells(self):
        for x in range(self.x0, self.x1 + 1):
            for y in range(self.y0, self.y1 + 1):
                yield x, y

    def centre(self):
        return ((self.x0 + self.x1 + 1) / 2, (self.y0 + self.y1 + 1) / 2)
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/scripts/make_levels.py#L47-L72){ .excerpt-source }

For each level it carves the rooms and corridors out of solid wall, gives
every wall the texture of the room it faces, hangs a door in the middle of
each corridor where walls face each other (locked where the layout says),
sets the exit switch, carves hidden supply rooms behind push walls where
there is solid rock enough, places enemies away from walls and at least
six units from the start, lamps along the walls, pickups against the
walls, intel on straight runs of wall, and checks: a solid border, every
room reachable from the start, nothing standing in a wall. Each placement
uses a generator seeded per level, so running the script again produces
the same files.

### The campaign and the story

`config.json`'s `campaign` lists the chapters in order, each with a card
(title, name, a few lines) and its levels; the campaign's level list is
the chapters' levels in turn. `story.h` decides what is told when:

- a **new game** opens with the opening pages, then the first chapter's
  card, then the first level's briefing;
- a level that **opens a chapter** is preceded by the chapter's card;
- after the **last level**, the ending pages, then the victory screen.

Pages are `std::string_view`s into the config, written into a fixed array
of eight; telling the story allocates nothing.

### Checking the shipped levels

`tests/level_design_test.cpp` loads every campaign level as shipped and
checks it is playable, whatever produced it: a solid border; every door
reachable; **every lock's key reachable without going through that lock**
(it simulates picking up keys as they become reachable); every enemy,
lamp, pickup and page of intel on reachable floor; no enemy within five
units of the start, and no lamp or pickup within one; an exit that can be reached; objectives, a briefing and
music; and every word of the story drawable with the glyphs the menu
rasterises.

## Design decisions and trade-offs

- **Everything read at startup.** A level starts without touching a file,
  so there is no hitch between levels and nothing to fail mid-game.
- **Two parsers.** The tree API for the small, deeply nested config (easy
  to write); SAX for level files (no tree built), where each value goes
  straight into its final place.
- **Generated levels, checked twice.** The script checks what it builds;
  the C++ test checks what ships, so a hand edit is checked too.
- **Seeded randomness everywhere.** Level generation, enemy patrols and
  drops are all seeded: two runs are the same run.

## Pitfalls

- **Coordinates are (row, column)** in the level files, the maps and the
  script alike (see [The map](map.md)).
- **Order matters for saves.** A saved game records pickups and enemies by
  their index in the level file (drops come after the level's own
  pickups). Regenerating a level can reorder them; `SavedGame::kFormat` is
  bumped when a change makes old saves meaningless.
- **Text must be drawable.** The menu rasterises printable ASCII and a few
  symbols (such as "·"); a curly quote or a long dash in a briefing would
  draw as "?". The level-design test catches it.

## Possible improvements

- A schema (JSON Schema) for `config.json` and the level files, so editors
  can validate them before the game does.
- Hot-reloading a level file in debug builds, for designing levels without
  restarting.
