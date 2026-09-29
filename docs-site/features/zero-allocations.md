# Zero allocations per frame

| | |
| --- | --- |
| **When** | 21 to 25 September 2026 |
| **Commits** | `59c7dfd` Add a reproducible web benchmark, `3d0c8ab` Attribute each frame's heap allocations to profiler sections, `f786c53` allocation-free grid A*, `e521199` Stop per-frame allocations in the camera, render queue and HUD, `3faf914` Reach zero heap allocations per frame and enforce it in CI, `1e1bbc5` Add a memory module, `8ffbef1` Put each level in an arena, `c2ac155` Count every allocation on the web and gate it in CI, `8d46824` Stop the menu allocating every frame, `bd577fd` level-load allocations, `6ecde29` no allocation after startup |
| **Code today** | [Memory](../engine/memory.md) |

## Problem

The game allocated **299 times per frame** on the web ("matching the web
build's 299 per frame", `3d0c8ab`). Each allocation is a call into a
general-purpose allocator whose cost varies, and in the browser the heap
can only grow.

## Constraints

- Measure first: "Measuring before optimising: this adds the tooling to see
  where frame time and memory go" (`59c7dfd`).
- Keep behaviour identical while changing data structures.
- Make it stay fixed: a CI gate, not a one-off cleanup.

## Approach

1. **Attribute** (`3d0c8ab`): every profiler section reads the allocation
   counter; "pathfinding 250 allocs/frame (84%) ... camera 30 ... render
   walls 12 ... render HUD 7".
2. **Pathfinding** (`f786c53`): 250 to 0, with a new A*.
3. **Copies and containers** (`e521199`): "Copies through `auto` without
   `&`: the raycaster copied the map's rows (27 allocations) and both
   renderers copied all 600 rays (~70 KB) every frame"; a
   `std::priority_queue` rebuilt each frame became a reused vector; HUD
   digits in a `std::array` instead of `std::list` nodes.
4. **States** (`3faf914`): enemies and weapons "creating a new state object,
   animation, frame list and name string on every transition" now own every
   state as a member. "Result: none of the 1940 measured frames of the
   benchmark scenario allocates (49 before the previous commit, 299
   originally)." CI fails if any frame after the warm-up allocates.
5. **Lifetimes** (`1e1bbc5`, `8ffbef1`): a monotonic arena per level and
   object pools; level load "drops from 653 heap allocations (361 KB) to
   254 (148 KB)".
6. **The web, fully counted** (`c2ac155`): counting `malloc` itself found
   what the native gate missed, "such as strings that fit the 22-character
   short-string buffer on 64-bit but not the 10-character one on wasm32".
7. **Everything after startup** (`6ecde29` and later): menus, level
   transitions, story pages, saving, dying: the soak session checks all of
   it.

## C++ techniques used

- [Polymorphic memory resources](../techniques/pmr-arenas.md),
  [object pools](../techniques/object-pool.md).
- [Replacing the allocator](../techniques/allocation-counting.md).
- [Text without allocating](../techniques/allocation-free-text.md),
  [strong types instead of strings](../techniques/strong-types.md).
- [Ranges algorithms](../techniques/ranges.md): `std::ranges::sort` with a
  tie-breaker instead of `std::stable_sort`'s buffer.

## Key code

- [`app/allocation_counter.cpp`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/app/allocation_counter.cpp)
- [`MonotonicArena`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Allocators/include/Allocators/monotonic_arena.h)
- [`scripts/check_soak.py`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/scripts/check_soak.py)

## Pitfalls

- **A gate only covers what it runs**: marks on walls were not measured
  until the benchmark shot walls (`7524277`).
- **SDL allocates on its first busy frame** unless warmed up at startup
  (see [The 3D renderer](../engine/renderer.md#warming-up-at-startup)).
- A bug surfaced on the way: "SoundManager ... used to hand out
  a new channel number per source forever, so after a level or two sounds
  went to channels that do not exist and silently did not play"
  (`3faf914`).

## What I'd change

- Track the arena's high-water mark per level in CI, to catch budgets
  that are much larger than needed.
