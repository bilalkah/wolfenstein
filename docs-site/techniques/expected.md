# Errors as values with `std::expected`

## The technique

`std::expected<T, E>` (C++23) holds either a value of type `T` or an error
of type `E`. It makes failure part of a function's type: the caller must
look before using the value, and errors travel as ordinary return values,
without exceptions and without out-parameters.

```cpp
std::expected<Map, std::string> FromFile(const std::string& path);

auto map = Map::FromFile(path);
if (!map) {
    std::cerr << map.error() << '\n';   // what went wrong
    return;
}
use(*map);                              // the value
```

`std::unexpected(error)` builds the error side. Where exceptions are for
the exceptional, `expected` suits failures that are a normal outcome of an
operation: a missing file, a malformed level, a full pool.

## Where it appears here

About forty functions return `std::expected`; the pattern is uniform from
the file parsers up to the `World`:

| Function | Value | Error |
| --- | --- | --- |
| `Map::FromFile` | the map | which file and row is wrong |
| `ParseLevel`, `ParseGameConfig` | typed level or config data | the JSON path of a missing or wrong field |
| `SceneLoader::Open` | every level prepared | which file failed |
| `World::Create`, `NewGame`, `ContinueGame`, `NextLevel` | the world, or nothing (`expected<void, std::string>`) | why the game cannot start |
| `TextureManager::Load`, `SoundManager::Open` | the manager | the image or sound that failed |
| `ObjectPool<T>::Create` | a `Handle<T>` | `PoolError::Full` |

Errors propagate by returning them:

```cpp title="src/Core/src/scene_loader.cpp"
std::expected<SceneLoader, std::string> SceneLoader::Open(
    std::string asset_dir) {
    auto config = ParseFile(asset_dir + "levels/config.json", ParseGameConfig);
    if (!config) {
        return std::unexpected(config.error());
    }
    SceneLoader loader(std::move(asset_dir), std::move(*config));
    for (const std::string& file : loader.config_.levels) {
        if (auto prepared = loader.Prepare(file); !prepared) {
            return std::unexpected(prepared.error());
        }
    }
    if (auto prepared = loader.Prepare(loader.config_.benchmark_level);
        !prepared) {
        return std::unexpected(prepared.error());
    }
    return loader;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/scene_loader.cpp#L56-L73){ .excerpt-source }

At the top, `Game::Init` prints the error and exits: a game that cannot
load its content cannot run. In tests, the same functions let a test
assert on the error message (`EXPECT_NE(error.find("pickup position"),
std::string::npos)`).

### Where exceptions remain

- nlohmann/json throws on type errors; the config parser catches them at
  one place and turns them into `std::expected` errors naming the field.
- The level arena throws `std::bad_alloc` when a computed budget is wrong:
  a programming error, not an outcome to handle.
- `ObjectPool::Create` rethrows if the object's constructor throws, after
  restoring the pool (the strong guarantee).

## Pitfalls

- `*result` on an error is undefined behaviour, like dereferencing an
  empty `std::optional`; `clang-tidy`'s checks and the habit of testing
  first keep it honest. Tests use `value_or` where a default makes sense.
- An `std::expected<void, E>` has no value to use; checking it is easy to
  forget. The engine returns it from functions whose callers always act on
  failure (starting a game, a level).
