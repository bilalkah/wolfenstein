# Strong types with `enum class`

## The technique

A plain `int` or `std::uint32_t` says nothing about what it means, and
nothing stops passing an object id where a sound channel is expected.
A **strong type** wraps the value in a distinct type. The lightest way in
C++ is a scoped enumeration with a fixed underlying type and *no
enumerators* (or only special ones):

```cpp
enum class ObjectId : std::uint32_t { None = 0xFFFF'FFFF };
ObjectId id{42};                        // list-initialised from the underlying type
std::size_t index = std::to_underlying(id);  // C++23: back to the integer
```

`enum class` values do not convert implicitly to integers or to each
other, so mixing them up is a compile error, and they cost nothing at
run time.

## Where it appears here

| Type | Underlying | What it prevents |
| --- | --- | --- |
| `ObjectId` | `std::uint32_t` | Passing any integer as an object's index into per-object arrays |
| `SoundChannel` | `int` | Mixing a sound source's channel with other integers |
| `SoundEffect` | `std::uint8_t` | Strings for sound names; indexes the array of loaded chunks |
| `KeyColour` | `std::uint8_t` | Keys as magic numbers; `KeyBit()` turns one into a bit of a set |
| `ProfileSection`, `ObjectType`, `Record`, `Fade`, `GameState`, the state enums | `std::uint8_t` | Small, typed choices |

```cpp title="src/SoundManager/include/SoundManager/sound_manager.h"
// The mixer channel a sound source plays on: a new sound from the source
// cuts off its previous one, never another source's
enum class SoundChannel : int {};
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/SoundManager/include/SoundManager/sound_manager.h#L86-L88){ .excerpt-source }

`std::to_underlying` (C++23) appears wherever a strong value becomes an
index: `ToIndex(ObjectId)`, `chunks_[std::to_underlying(effect)]`. An
`static_assert` keeps the effect count in step with the enum:

```cpp title="src/SoundManager/include/SoundManager/sound_manager.h"
inline constexpr std::size_t kSoundEffectCount = 36;
static_assert(std::to_underlying(SoundEffect::PlasmaBurst) + 1 ==
              kSoundEffectCount);
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/SoundManager/include/SoundManager/sound_manager.h#L72-L74){ .excerpt-source }

### Enums instead of strings

Sounds were once played by name. On wasm32, libc++'s short-string buffer
holds only 10 characters, so a name like `"npc_attack"` fit natively but
**allocated in the browser** every time a sound played. An enum indexing
an array is free on both (`8ffbef1`).

## Pitfalls

- An `enum class` with no enumerators accepts any value of its underlying
  type through `T{value}`; the type prevents mix-ups, not bad values.
- The small `std::uint8_t` underlying types keep structs compact, but
  `-Wconversion` then asks for explicit casts when doing arithmetic on
  them; the code uses `static_cast` in those places.
