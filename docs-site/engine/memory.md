# Memory

## Purpose

After startup, this game makes **no heap allocation at all**: not per
frame, not when a level starts, not when an enemy dies or a sound plays,
natively or in the browser. This page explains why that matters, how the
engine gets there (two allocators, reserving up front, never copying on
hot paths), and how it is measured and enforced.

Code: `src/Allocators/` (`monotonic_arena.h`, `object_pool.h`,
`asan.h`), `app/allocation_counter.cpp`, `src/Profiler/`, and the scene and
navigation containers.

## Concepts

### Why avoid allocating

A general-purpose allocator (`new`, `malloc`) takes a lock or a
thread-local cache, searches free lists, may ask the operating system for
pages, and fragments memory over time. Each call is fast on average but
unpredictable, and a frame has about 16 ms. In WebAssembly the heap is one
linear memory that can only grow (`ALLOW_MEMORY_GROWTH`), and growing it
copies nothing but can stall. Game engines therefore manage memory by
**lifetime**: allocate what lives as long as the program at startup, what
lives as long as a level in a level arena, and reuse fixed pools for
things that come and go.

### Arenas (bump allocators)

An arena takes one block and hands out consecutive pieces by moving a
pointer: allocation is an add and an align, freeing an individual piece
does nothing, and everything is released at once by resetting the pointer.
It fits data that dies together, like everything belonging to one level.
C++17's `std::pmr` lets standard containers use such an allocator through
a `std::pmr::memory_resource`. See
[Polymorphic memory resources](../techniques/pmr-arenas.md).

### Pools with generational handles

A pool keeps fixed-size slots for one type and a free list; creating and
destroying objects is O(1) with no allocation. A **handle** (slot index +
generation) instead of a raw pointer makes a reference to a destroyed
object detectable: destroying bumps the slot's generation, and a stale
handle no longer matches. See
[Object pools and generational handles](../techniques/object-pool.md).

## How it is implemented here

### Lifetimes and where memory comes from

| Lifetime | Where | Examples |
| --- | --- | --- |
| Program | Heap, at startup | Textures, sounds, the config, every prepared level, renderers, UI glyphs |
| Game | In place, in `World` | The player and its weapons (`std::optional<Player>`) |
| Level | The `World`'s `MonotonicArena`, reset per level | Map cells, object pools, object lists, noise buffers, navigation grid and routes, door states |
| Frame | Members reused with their capacity, and the stack | The render queue, rays, commands, formatted text |

### The arena

```cpp title="src/Allocators/include/Allocators/monotonic_arena.h"
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        assert(std::has_single_bit(alignment) &&
               "alignment must be a power of two");
        // Every allocation gets a distinct address, even an empty one
        bytes = std::max<std::size_t>(bytes, 1);

        const auto begin = std::bit_cast<std::uintptr_t>(begin_);
        auto top = std::bit_cast<std::uintptr_t>(current_);
        if (bytes > top - begin) [[unlikely]] {
            throw std::bad_alloc();
        }
        top = (top - bytes) & ~(alignment - 1);
        if (top < begin) [[unlikely]] {
            throw std::bad_alloc();
        }
        current_ = begin_ + (top - begin);
        UnpoisonRegion(current_, bytes);
        return current_;
    }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Allocators/include/Allocators/monotonic_arena.h#L69-L87){ .excerpt-source }

Three choices stand out, all explained in its header:

- It bumps **downwards**, from the end of the block: aligning the new
  pointer is then a single mask, `& ~(alignment - 1)`. (Measured: about
  2.6 times faster than bumping upwards and 1.25 times faster than
  `std::pmr::monotonic_buffer_resource`.)
- It **never grows**: running out throws `std::bad_alloc` instead of
  quietly falling back to the heap, so a blown budget is found, not hidden.
- Under AddressSanitizer, memory not currently handed out is **poisoned**,
  so an access after `Reset()` is reported like a use-after-free.

The `World` owns one arena sized for the **largest** level (computed while
the levels are prepared at startup), rewinds it for each level, and builds
the `Scene` inside it.

### The pools

```cpp title="src/Allocators/include/Allocators/object_pool.h"
    template <typename... Args>
    [[nodiscard]] std::expected<Handle<T>, PoolError> Create(Args&&... args) {
        if (free_list_.empty()) {
            return std::unexpected(PoolError::Full);
        }
        const std::uint32_t index = free_list_.back();
        free_list_.pop_back();
        UnpoisonRegion(&slots_[index], sizeof(Slot));
        try {
            std::construct_at(
                reinterpret_cast<T*>(slots_[index].storage.data()),
                std::forward<Args>(args)...);
        }
        catch (...) {
            // Strong guarantee: the pool is unchanged if T's constructor throws
            PoisonRegion(&slots_[index], sizeof(Slot));
            free_list_.push_back(index);
            throw;
        }
        alive_[index] = 1;
        return Handle<T>{index, generations_[index]};
    }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Allocators/include/Allocators/object_pool.h#L94-L115){ .excerpt-source }

The `Scene` keeps a pool each for enemies, lamps and pickups, sized from
the level's counts, with their storage in the arena. A full pool is an
ordinary outcome (`std::expected`), not an exception.

### Reserving everything up front

Most "zero allocations" come from discipline rather than allocators:

- every container that fills during a level is `reserve`d to its final
  size when the level starts (object lists, routes, scratch buffers, the
  A* heap to its exact bound of four pushes per cell plus one);
- containers that are refilled each frame are `clear()`ed, which keeps
  their capacity (the render queue);
- fixed arrays for what comes and goes (12 puffs, 16 projectiles, 32 wall
  marks, 8 pages of intel, 24 voices, 256 sound commands, 8 story pages);
- text is formatted into stack buffers (`FixedText`, `RecordWriter`), not
  `std::string`s (see [Text without allocating](../techniques/allocation-free-text.md));
- SDL's own buffers are grown once at startup by the renderer's warm-up
  (see [The 3D renderer](renderer.md#warming-up-at-startup)).

### Counting allocations

`app/allocation_counter.cpp` replaces the allocator in the executable and
counts every allocation in `AllocationStats`:

- **natively**, the global `operator new` and `delete`: the game's own C++
  allocations. (Headless native runs draw with SDL's software renderer,
  which allocates in every `SDL_RenderTexture`; counting that would measure
  the test setup.)
- **on the web**, `malloc` itself (through `emscripten_builtin_malloc` and
  friends), so SDL, SDL_mixer, SDL_ttf and the WebGL renderer, which never
  go through `operator new`, are counted too.

```cpp title="app/allocation_counter.cpp"
void Count(std::size_t size) {
    if (OnMainThread()) {
        wolfenstein::AllocationStats::count++;
        wolfenstein::AllocationStats::bytes += size;
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/app/allocation_counter.cpp#L50-L55){ .excerpt-source }

Every profiler section reads the counter as it begins and ends, so a
report says not only *that* a frame allocated but *which section* did.
See [Replacing the allocator](../techniques/allocation-counting.md).

### Enforced in CI

- The **benchmark** (a scripted walk through a level with enemies
  chasing) must report zero allocations in every frame after warm-up,
  natively and in headless Chromium (`scripts/alloc_breakdown.py
  --require-zero`).
- The **soak session** plays through every screen a player can reach (the
  3D view, the 2D view, the map, pause, settings, a pickup, a door, a level
  transition, dying, a new game) and must allocate nothing after the first
  game starts (`scripts/check_soak.py`).
- Unit tests built with `WOLFENSTEIN_COUNTS_ALLOCATIONS` assert zero
  allocations around specific operations (collecting pickups, reading
  intel, capturing a saved game, playing a level).

## Design decisions and trade-offs

- **Budgets computed, not guessed.** `Scene::MemoryFor` and
  `NavigationManager::MemoryFor` add up exactly what a level will take from
  the arena, alignment padding included, from the level's counts. The
  arena throws if the computation was wrong, and tests load every level.
- **Two counters, one per platform.** Native counts the game's code; web
  counts everything the shipped build does. Both are needed: the native
  gate "missed wasm-only allocations, such as strings that fit the
  22-character short-string buffer on 64-bit but not the 10-character one
  on wasm32" (`c2ac155`).
- **Startup may allocate freely.** Loading every level, texture and sound
  at startup is what makes the rest possible.

## Pitfalls

- **Copies through `auto`.** `auto rays = camera.GetRays();` copies 600
  rays; the fix was `const auto&` (`e521199` found several: the map's
  rows, all rays twice a frame, the enemy list on every shot).
- **Short strings are shorter on wasm32.** libc++'s small-string buffer
  holds 22 characters on 64-bit targets but 10 on wasm32: a sound or clip
  name that did not allocate natively did in the browser. The engine now
  uses enums and `string_view`s on hot paths.
- **`std::stable_sort` may allocate** a temporary buffer; the render queue
  uses `std::ranges::sort` with an explicit tie-breaker instead.
- **Arena objects are not destroyed by `Reset()`.** The owner (the
  `Scene`) must be destroyed first; `World::StartLevel` resets the scene
  before the arena.

## Possible improvements

- Report the arena's high-water mark per level in the benchmark output, to
  keep the budget honest over time.
- Count allocations on the audio thread too (the web counter skips other
  threads; the build is single-threaded, but a future threaded build would
  need atomic counters).
