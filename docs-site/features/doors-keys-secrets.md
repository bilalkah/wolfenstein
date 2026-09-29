# Doors, keys and secrets

| | |
| --- | --- |
| **When** | 26 September 2026 |
| **Commits** | `7d8bdc3` Add sliding doors, opened with E or Space, `d029813` Add gold and silver keys and the doors they unlock, `e0fb909` Hide supply rooms behind push-walls, `5594b55` Mark secret walls with a faint crack |
| **Code today** | [The map](../engine/map.md), [Raycasting](../engine/raycasting.md#a-door-is-a-plane) |

## Problem

Give levels the structure of *Wolfenstein 3D*: doors between rooms,
locked doors whose keys are elsewhere in the level, and secret walls that
slide back to reveal hidden supplies.

## Constraints

- The raycaster must draw a door part open and a wall part slid.
- Movement, sight, sound and shots must respect them.
- Enemies must route through doors they can open and round those they
  cannot.
- A secret should be findable by looking, not only by trying every wall.

## Approach

- **Doors** (`7d8bdc3`): "a plane across the middle of the cell between two
  walls, that slides aside as it opens. The raycaster stops on its closed
  part and passes through the open part, so the texture slides with it; a
  closed door blocks movement, sight and shots." Enemies open doors by
  walking up to them; a door closes after four seconds "never onto
  anyone".
- **Keys** (`d029813`): G and S cells; keys are pickups held for the level;
  "enemies never open locked doors, and plan their routes around them";
  "You need the gold key".
- **Secrets** (`e0fb909`): walls that slide two cells back when used; "while
  one slides it is a block between cells, which the raycaster meets after
  its grid walk, and its whole way blocks movement and sight; then it rests
  as an ordinary wall. Two cells, so the block does not stop in front of the
  gap it leaves."
- **The crack** (`5594b55`): "A secret wall looked exactly like its
  neighbours, so finding one meant trying every wall. It now shows a jagged
  hairline crack over a faintly worn patch in the lower middle of its
  face."

## C++ techniques used

- Door state in a side table (`std::pmr::vector<Door>`), cells encoding
  `kDoorCell + index`.
- The slab method (ray against an axis-aligned box) for a sliding wall, in
  `HitMovingPushWalls`.

## Key code

- [`HitDoor`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Camera/src/raycaster.cpp#L21-L44)
  and [`HitMovingPushWalls`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Camera/src/raycaster.cpp#L48-L95)
- [`Map::IsBlocked`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/GameMap/src/map.cpp#L160-L178)
- [`Map::AdvancePushWalls`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/GameMap/src/map.cpp#L231-L246)

## Pitfalls

- "The 3D view's draw queue now has room for two commands a column (a wall
  and its mark) and the largest level's objects, told once at startup: it
  had room for one a column and 64 more, so facing a secret up close would
  have grown it" (`5594b55`), which would have allocated mid-game.
- Pathfinding must be refreshed where a secret stops, or enemies would
  treat the opened room as solid.

## What I'd change

- Doors with a lock but no key in the level are rejected by the tests;
  switch-operated doors would need a new kind of trigger.
