# Non-owning views

## The technique

A **view** refers to data owned elsewhere without copying it:

- `std::string_view` (C++17): a pointer and a length into characters;
- `std::span<T>` (C++20): a pointer and a count into a contiguous array of
  `T` (a `std::vector`, a `std::array`, a C array, part of any of them).

Views are cheap to pass by value and make function signatures honest
("I read this, I do not keep it"). Their one rule: the view must not
outlive what it refers to.

## Where it appears here

Views are the default currency between subsystems: about 150 uses of
`std::string_view` and 70 of `std::span` in `src/` and `app/`.

- **Getters return spans, not containers.** `Scene::GetObjects()`,
  `GetEnemies()`, `GetPickups()`, `GetWallMarks()`, `GetIntel()`,
  `Map::GetDoors()`, `GetPushWalls()`: callers loop over the scene's own
  storage, and no getter copies a vector (copies through `auto` without `&`
  were a source of per-frame allocations, removed in `e521199`).
- **Animations are spans into the texture manager.** A `LoopedAnimation`
  holds a `std::span<const std::uint16_t>` of frame ids that the
  `TextureManager` keeps for the program's life, so every enemy playing
  "soldier_walk" shares one list, and building an animation allocates
  nothing.
- **Text is `string_view`** wherever it is only read: level names,
  briefings, objectives, story pages (`StoryPage` is three views into the
  game's config), the menu's labels, sound and clip names looked up by
  name.
- **Parts of arrays**: `NavigationManager::FindPath` takes
  `std::span(cells_).subspan(1)` (the path without its start cell) and
  copies `path.first(route.size)` into the route.
- **Buffers for writers**: `RecordWriter` writes into a
  `std::span<char>` the caller owns (a stack array).

```cpp title="src/Core/include/Core/story.h (excerpt)"
struct StoryPage
{
    std::string_view heading;
    std::string_view title;
    std::string_view text;
};
```

## Lifetimes

Every view in the engine points into something that outlives it by
design:

| Views into | Live as long as |
| --- | --- |
| `GameConfig` strings (story, names, briefings) | the program (the `SceneLoader` inside the `World`) |
| The `TextureManager`'s clips | the renderer context (the whole program) |
| A `Scene`'s objects and maps | the level; views are re-pointed at each level change |
| A stack buffer (`FixedText`, `RecordWriter`) | the statement or function that draws or saves |

`SoundManager::PlayMusic(std::string_view name)` documents the one place
a view is kept: "The name must outlive the manager (a level's, from its
data)."

## Pitfalls

- **A view of a temporary dangles.** `std::string_view name =
  std::string("x") + y;` points into a string destroyed at the end of the
  statement. Clang's `-Wdangling` catches the obvious cases; the rest is
  discipline (and AddressSanitizer in CI).
- **A span of a vector dangles when the vector grows.** Level containers
  are reserved to their final size before anything takes a span of them,
  and never grow during the level.
- **`string_view` is not NUL-terminated**: passing `.data()` to a C API
  (SDL, a JavaScript function via `EM_JS`) needs a terminated copy; the
  record writer keeps its buffer NUL-terminated for that reason.
