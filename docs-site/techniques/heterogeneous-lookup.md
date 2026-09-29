# Heterogeneous lookup

## The technique

A `std::map<std::string, T>` looked up with a `std::string_view` or a
string literal normally constructs a temporary `std::string` for the key,
which may allocate. **Heterogeneous lookup** lets the container compare
the probe directly with its keys:

- for ordered containers (C++14), use a **transparent comparator**:
  `std::map<std::string, T, std::less<>>`; `std::less<void>` compares any
  two types with `<` and declares `is_transparent`;
- for unordered containers (C++20), both the hash and the equality must be
  transparent: a hash with `using is_transparent = void;` that accepts
  `std::string_view`, and `std::equal_to<>`.

Then `find(std::string_view{...})` works without building a string.

## Where it appears here

The game configuration's maps are ordered and transparent:

```cpp title="src/Core/include/Core/level_data.h"
    // By enemy type ("soldier"); std::less<> allows string_view lookups
    std::map<std::string, EnemyConfig, std::less<>> enemies;
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Core/include/Core/level_data.h#L100-L101){ .excerpt-source }

The same goes for pickups and for the loader's prepared levels
(`std::map<std::string, PreparedLevel, std::less<>>`), so
`FindLevel(std::string_view)` and `pickups.find(type)` never allocate.

The texture manager's name tables are unordered and transparent:

```cpp title="src/TextureManager/include/TextureManager/texture_manager.h"
    struct StringHash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view key) const noexcept {
            return std::hash<std::string_view>{}(key);
        }
    };
    template <typename V>
    using StringMap =
        std::unordered_map<std::string, V, StringHash, std::equal_to<>>;
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/TextureManager/include/TextureManager/texture_manager.h#L125-L134){ .excerpt-source }

`GetTextureCollection(std::string_view)` finds a clip by name without a
temporary string. `LoopedAnimation` builds side names like
`"soldier_walk@3"` in a stack buffer and looks them up as views: a level
loads its enemies' animations without allocating.

## Why it matters here

Most lookups by name happen at load, where allocating is allowed; but a
level load was one of the paths made allocation-free (`8ffbef1`,
"take strings and copies off the hot paths"), and on wasm32 any name
longer than 10 characters allocated as a temporary `std::string`.

## Pitfalls

- A transparent hash must hash a `std::string` key and a
  `std::string_view` probe with the same characters to the same value;
  hashing everything as `std::string_view` guarantees it.
- Only `find`, `count`, `contains`, `equal_range` (and, in C++23 for
  ordered maps, more) take heterogeneous keys; `operator[]` and `at`
  still build a key.
