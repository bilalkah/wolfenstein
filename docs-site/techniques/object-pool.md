# Object pools and generational handles

## The technique

An **object pool** reserves storage for a fixed number of objects of one
type and hands out slots: creating an object constructs it in a free slot,
destroying it runs its destructor and returns the slot to a free list.
Both are O(1) and touch no allocator.

Code that refers to pooled objects by raw pointer has a problem: after an
object is destroyed and its slot reused, an old pointer points at a
different object (a use-after-free that no tool will flag, because the
memory is valid). A **generational handle** fixes it: a handle is (slot
index, generation). Each slot keeps a generation counter, bumped on every
destroy; the pool resolves a handle only if the generations match, and
returns null for a stale one.

Building objects in raw storage needs two C++20 tools:

- `std::construct_at(ptr, args...)` constructs an object at an address (a
  `constexpr`-friendly placement `new`);
- `std::launder(ptr)` tells the compiler that the storage now holds a
  *new* object, so a pointer obtained from the storage's address may be
  used to access it (without it, the compiler may assume the old object's
  const or reference members did not change).

## Where it appears here

`memory::ObjectPool<T>` (`src/Allocators/include/Allocators/object_pool.h`)
backs the scene's enemies, lamps and pickups:

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
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Allocators/include/Allocators/object_pool.h#L94-L115){ .excerpt-source }

Design points, from its header and commit `1e1bbc5`:

- **Storage from any `std::pmr::memory_resource`** (the level arena), taken
  once, as an array of `Slot { alignas(T) std::array<std::byte, sizeof(T)> }`.
- **Bookkeeping apart from the objects**: generations, liveness and the free
  list are separate arrays, so validating a handle does not pull the object
  into cache. Liveness is one byte per slot rather than
  `std::vector<bool>`, "whose packed bits turn every check into a shift
  and a mask behind a proxy reference".
- **A full pool is a value** (`std::expected<Handle<T>, PoolError>`), and
  `[[nodiscard]]` makes ignoring the result a warning.
- **The strong exception guarantee**: if `T`'s constructor throws, the pool
  is exactly as before.
- **AddressSanitizer poisoning** of free slots: touching a destroyed object
  is reported like a use-after-free, even though the memory is the pool's.
- **Handles are two `uint32_t`s**: "cheap to copy, compare and send over a
  network".

## How the engine actually uses handles

Within a level nothing is destroyed (see
[Game objects and the scene](../engine/entities.md)), so the scene keeps
raw pointers to its pooled objects in its lists and uses the pool mainly
for its storage and construction. The handles and generations earn their
keep in the tests (`tests/object_pool_test.cpp` checks stale handles) and
leave room for objects that come and go.

## Pitfalls

- `reinterpret_cast` of the slot's bytes is only valid for reading the
  object through `std::launder`; the pool's `Object(index)` does exactly
  that, in one place.
- A pinned `T` (copy and move deleted) works, since `construct_at` builds
  in place; that is how enemies, whose states point back to them, live in
  a pool.
