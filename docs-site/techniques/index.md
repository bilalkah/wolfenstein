# C++ techniques used here

The engine is written in C++23 and built with Clang and libc++ under a
strict warning set, `clang-tidy`, and AddressSanitizer and
UndefinedBehaviorSanitizer in CI. Each page in this section explains one
technique in general, then shows where and why it appears in this code.
Only techniques the code actually uses are listed; a count of uses across
`src/` and `app/` at the documented commit is given where it helps.

| Technique | Where it matters most | Standard |
| --- | --- | --- |
| [Special members and pinned types](special-members.md) | Every class that owns something or is pointed into; `tests/type_traits_test.cpp` | C++11 and later |
| [RAII and custom deleters](raii.md) | SDL resources, `ScopedTimer`, member order as destruction order | C++11 |
| [Errors as values with `std::expected`](expected.md) | Loading files, levels, the world; pools that are full (about 40 uses) | C++23 |
| [Non-owning views](views.md) | `std::span` (about 70 uses) and `std::string_view` (about 150) across every interface | C++17, C++20 |
| [Polymorphic memory resources](pmr-arenas.md) | The level arena and every level container | C++17 |
| [Object pools and generational handles](object-pool.md) | Enemies, lamps and pickups; `std::construct_at`, `std::launder` | C++20 |
| [`std::mdspan`](mdspan.md) | The map's cells, `cells[x, y]` | C++23 |
| [State machines with templates](state-machines.md) | Enemies and weapons; a type trait maps an owner to its states | C++17 |
| [Strong types with `enum class`](strong-types.md) | `ObjectId`, `SoundChannel`, `SoundEffect`, `KeyColour`, `std::to_underlying` | C++11, C++23 |
| [Ranges algorithms](ranges.md) | Sorting the render queue with a static lambda, the A* heap, projections | C++20, C++23 |
| [Compile-time code and attributes](compile-time.md) | About 220 `constexpr`, `static_assert`, `[[nodiscard]]`, `[[unlikely]]`, `std::unreachable` | C++11 to C++23 |
| [Text without allocating](allocation-free-text.md) | `std::format_to_n`, `std::to_chars` | C++17, C++20 |
| [Heterogeneous lookup](heterogeneous-lookup.md) | Maps keyed by `std::string`, looked up with `std::string_view` | C++14, C++20 |
| [Atomics and a lock-free ring](lock-free-ring.md) | The game thread and the audio thread | C++11 |
| [Streaming JSON with SAX](sax-parsing.md) | Reading level files without a tree | (library) |
| [Replacing the allocator](allocation-counting.md) | Counting every allocation, natively and in the browser | C++ and C |
| [Templates, concepts and forwarding](templates.md) | Pools, the path finder's grid, formatted text, record writing | C++11 to C++20 |

## Things the code deliberately does not use

- **`std::shared_ptr`**: none. Ownership is single and explicit; a
  `shared_ptr` cycle between states and their owners once leaked every
  enemy (`39846c6`).
- **Exceptions for expected failures**: a missing file, an unknown level, a
  full pool come back as values (`std::expected`). Exceptions remain for
  the impossible (`std::bad_alloc` from a blown arena budget).
- **`std::function`, `std::variant`, coroutines**: not used.
- **Heap allocation after startup**: never (see [Memory](../engine/memory.md)).
