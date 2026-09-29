# Replacing the allocator

## The technique

C++ lets a program **replace the global allocation functions**:
defining `operator new(std::size_t)` and `operator delete(void*)` (and
their array and sized forms) in the program replaces the library's for
every `new` and `delete` in it. The replacement must be defined in the
executable (in a static library, the linker may not pull it in), must
return suitably aligned memory, and must throw `std::bad_alloc` on
failure.

C code (and C++ code calling `malloc`) does not go through `operator new`.
To see *every* allocation, `malloc`, `calloc`, `realloc`, `free` and the
aligned variants must be replaced too, which is platform-specific:
Emscripten provides `emscripten_builtin_malloc` and friends to call the
real allocator from a replacement.

## Where it appears here

`app/allocation_counter.cpp` is linked into the executable (except in
sanitizer builds, whose runtime replaces the allocator itself) and counts
into `AllocationStats::count` and `::bytes`:

- **Natively** it replaces `operator new`, `new[]`, `delete` and
  `delete[]` (and the sized deletes), calling `std::malloc` and
  `std::free`: the game's own C++ allocations.
- **On the web** it replaces `malloc`, `calloc`, `realloc`, `memalign`,
  `aligned_alloc`, `posix_memalign` and `free`, forwarding to
  `emscripten_builtin_*`. `operator new` keeps its default definition,
  which calls `malloc`, so C++ allocations are counted too, and so is
  everything SDL, SDL_mixer, SDL_ttf and the WebGL renderer do.

```cpp title="app/allocation_counter.cpp"
void Count(std::size_t size) {
    if (OnMainThread()) {
        wolfenstein::AllocationStats::count++;
        wolfenstein::AllocationStats::bytes += size;
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/app/allocation_counter.cpp#L50-L55){ .excerpt-source }

Why two strategies: headless native runs (CI, the dev container) draw with
SDL's *software* renderer, which allocates inside every `SDL_RenderCopy`;
counting that would measure the test setup, not the game. In a real
browser, the shipped configuration, counting at the `malloc` level is
exactly right, and it found allocations the native count missed (strings
past wasm32's 10-character short-string buffer, `c2ac155`).

The counters are `static inline` members of `AllocationStats` (C++17
inline variables: one definition across the program, in a header). Every
`ScopedTimer` reads them as it starts and ends, so allocations are
attributed to profiler sections (see
[Profiling and testing](../engine/profiling.md)).

## Pitfalls

- **Sanitizers replace the allocator too.** AddressSanitizer's runtime
  intercepts `malloc` and `new`; linking a second replacement clashes, so
  sanitizer builds leave the counter out (`CMakeLists.txt`).
- **Only the main thread is counted** on the web: the counters are not
  atomic. The game is single-threaded there.
- **The replacement is the allocator**: it cannot use RAII or containers
  itself (`clang-tidy`'s ownership checks are silenced for that file with
  `NOLINTBEGIN`/`NOLINTEND` and a comment saying why).
