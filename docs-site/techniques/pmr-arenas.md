# Polymorphic memory resources

## The technique

Standard containers take an **allocator** as a template parameter, which
makes a `std::vector<int, MyAlloc>` a different type from a
`std::vector<int>`: code taking one cannot take the other. C++17's
`std::pmr` ("polymorphic memory resource") moves the choice to run time:

- `std::pmr::memory_resource` is an abstract class with `allocate`,
  `deallocate` and `is_equal` (implemented by overriding `do_allocate`,
  `do_deallocate` and `do_is_equal`);
- `std::pmr::vector<T>` (and `string`, `map`, ...) is the ordinary
  container with `std::pmr::polymorphic_allocator<T>`, which forwards to a
  `memory_resource*` given at construction;
- the standard library provides resources such as
  `monotonic_buffer_resource` and `unsynchronized_pool_resource`.

All `std::pmr::vector<T>` have one type whatever memory they use, at the
cost of a virtual call per allocation (not per element access).

## Where it appears here

The engine writes its own resource, `memory::MonotonicArena` (a
`std::pmr::memory_resource` whose `do_allocate` bumps a pointer down a
fixed block; see [Memory](../engine/memory.md)), and every container whose
lifetime is a level takes it:

- `Map`'s cells, doors and push walls (`std::pmr::vector`), copied into the
  arena by `Map(const Map&, std::pmr::memory_resource*)`;
- the `Scene`'s object lists, explored flags, noise buffers and door
  states;
- the `ObjectPool`s' slot storage and bookkeeping;
- the navigation grid, the A* arrays and heap, the enemies' routes.

Example, from the scene's constructor: every member is built with
`&arena_`:

```cpp
explored_(std::size_t{map.GetSizeX()} * map.GetSizeY(), 0, &arena_),
noise_distance_(std::size_t{map.GetSizeX()} * map.GetSizeY(), kUnheard,
                &arena_),
```

### Why not `std::pmr::monotonic_buffer_resource`?

The standard one grows: when its buffer runs out it quietly asks its
upstream resource (the heap, by default) for more. The engine's arena
"never grows: running out throws `std::bad_alloc` instead of silently
falling back to the heap, so a blown memory budget is found, not hidden",
rewinds for the next level with `Reset()`, reports a high-water mark, and
poisons released memory under AddressSanitizer. It is also faster: bumping
downwards makes alignment a single mask (measured about 1.25 times faster
than the standard resource, in `benchmarks/micro/memory_benchmark.cpp`).

## Pitfalls

- **Assignment across resources copies.** Moving a `pmr::vector` into one
  with a different resource cannot steal the buffer; it copies element by
  element (and allocates). `Map` deletes its assignment operators for this
  reason: "assigning across memory resources would copy (and could
  throw) where a move is expected not to".
- **The resource must outlive the containers.** The `World` declares its
  arena before the `Scene` that uses it, so the scene (and its containers)
  are destroyed first.
- **Deallocation is a no-op** in a monotonic arena: a container that grows
  and shrinks repeatedly would leak arena space. Level containers are
  reserved once and never grow.
