# Glossary

Terms used across this site, in the sense the engine uses them.

**Alpha (interpolation)**
:   How far a drawn frame is between the last simulation tick and the next,
    from 0 to 1 (`FixedStep::Alpha`). Moving things are drawn that far from
    their previous pose to their latest.

**Arena (monotonic)**
:   An allocator over one block that hands out consecutive pieces and frees
    everything at once. Here, one per level (`memory::MonotonicArena`).

**Billboard**
:   A flat picture that always faces the viewer, standing in the world as
    a sprite: enemies, lamps, pickups, puffs, projectiles.

**Cell**
:   One square of the level's grid, one unit wide. `(x, y)` is (row,
    column). A cell is floor, a wall (with a texture), or a door.

**Command (`PlayerCommand`)**
:   What the player wants to do for one tick: move, strafe, turn, look,
    fire, reload, use, switch weapons. The only input the simulation sees.

**DDA (digital differential analyser)**
:   Walking a ray through a grid one cell at a time, always crossing the
    nearer of the next vertical and horizontal grid lines.

**Decal**
:   A picture on a wall's face (a bullet mark, a secret wall's crack, a
    page of intel), drawn as one textured quad over the wall's columns.

**Determinism**
:   The same commands from the same start always give the same game; the
    fixed tick and seeded random generators make it so.

**Door**
:   A door cell: a plane across the middle of the cell that slides aside
    as it opens; optionally locked with the gold or silver key.

**Drop**
:   A pickup an enemy carries and leaves where it dies; made, hidden, when
    the level loads, and rolled from the game's seed.

**Fishbowl effect**
:   Walls bulging towards the middle of the screen when heights come from
    the distance along each ray; fixed by using the perpendicular distance.

**Fixed timestep (tick)**
:   A simulation step of a constant 1/60 s, however long frames take.

**Frame**
:   One picture drawn on the screen.

**Generation (handle, stamp)**
:   A counter that makes an old reference detectable: a pool slot's
    generation (stale handles), or A*'s per-query stamp (entries from
    previous queries).

**Hitscan**
:   A shot resolved instantly along a line, as opposed to a projectile
    that flies.

**Hit zone**
:   The head, body or legs of a figure, which scale a hit's damage.

**Intel**
:   A page of the story pinned to a wall, read by walking up to it.

**Level (scene)**
:   One map with its enemies, objects, doors and secrets, owned by a
    `Scene` for as long as it is played.

**Mask (sprite)**
:   One bit per pixel of a sprite frame, set where the picture shows;
    shots hit only there.

**Noise**
:   A flood fill from a gunshot through open cells, alerting the enemies
    it reaches.

**Painter's algorithm**
:   Drawing from far to near so nearer things cover farther ones, without
    a depth buffer.

**Perpendicular distance**
:   The distance from the camera plane to a point, \(t\cos(\phi - \theta)\)
    for a point \(t\) along a ray at angle \(\phi\) with the view at
    \(\theta\).

**Pinned (type)**
:   A type whose copy and move operations are deleted because other objects
    point into it.

**Pitch**
:   How far the view is tipped up or down, as a share of the screen's
    height; drawn by sliding the world down or up (shearing).

**Pixels per unit**
:   How many screen pixels an object one unit tall spans one unit away:
    the screen's height at the base field of view, and less as the view
    widens, by \(\tan(\text{FOV}_{base}/2) / \tan(\text{FOV}/2)\).

**Pointer lock**
:   The browser API that hides the cursor and reports raw mouse motion; the
    web build's equivalent of relative mouse mode.

**Pool (object pool)**
:   Fixed-capacity storage for objects of one type, with O(1) create and
    destroy and no allocation.

**Push wall (secret)**
:   A wall that slides two cells back when used, opening a hidden room.

**Render queue**
:   The frame's list of draw commands (wall strips, sprites, decals, the
    weapon), sorted by distance and drawn back to front.

**Seed**
:   The number a game's random rolls (the enemies' drops) derive from;
    kept in saved games.

**Soak session**
:   A scripted run through every screen a player can reach, checking that
    nothing allocates after startup.

**SPSC ring**
:   A single-producer, single-consumer ring buffer: lock-free
    communication between exactly two threads.

**Tick**
:   One step of the simulation (1/60 s).

**View angles**
:   The direction the player looks (turn and pitch), kept by the
    presentation, turned every frame by the input and taken by the
    simulation at each tick.

**WASM (WebAssembly)**
:   The portable binary format the game is compiled to for browsers, by
    Emscripten.
