# Templates, concepts and forwarding

## The technique

- **Class and function templates** write code once for many types.
- **Variadic templates** (`template <typename... Args>`) take any number of
  arguments; **perfect forwarding** (`Args&&... args` with
  `std::forward<Args>(args)...`) passes them on as they were given, lvalues
  as lvalues and temporaries as temporaries, so a wrapper constructs an
  object exactly as if the caller had.
- **Concepts** (C++20) constrain template parameters with named
  requirements: `template <std::predicate<int, int> F>` accepts only
  callables that take two `int`s and return something convertible to
  `bool`, and a wrong argument fails with a readable error at the call.

## Where it appears here

### Constructing in place, forwarding the arguments

`ObjectPool<T>::Create(Args&&... args)` forwards its arguments to
`std::construct_at`, so `enemies_.Create(*this, config, position)` builds
an `Enemy` in its slot with exactly those arguments (see
[Object pools](object-pool.md)). `FixedText`'s constructor forwards its
arguments to `std::format_to_n` the same way (see
[Text without allocating](allocation-free-text.md)).

### A constrained callback

```cpp title="src/NavigationManager/include/NavigationManager/grid_path_finder.h"
    template <std::predicate<int, int> IsWall>
    void SetGrid(int height, int width, IsWall&& is_wall) {
        Resize(height, width);
        for (int x = 0; x < height; ++x) {
            for (int y = 0; y < width; ++y) {
                walls_[static_cast<std::size_t>(Index({x, y}))] =
                    is_wall(x, y) ? 1 : 0;
            }
        }
    }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/NavigationManager/include/NavigationManager/grid_path_finder.h#L56-L65){ .excerpt-source }

The path finder fills its grid by asking a callable whether each cell is a
wall. The navigation manager passes a lambda that consults the map; the
path finder never sees the `Map` type, needs no intermediate copy of the
grid, and the lambda is inlined. Using a template instead of
`std::function` avoids type erasure (and its possible allocation).

### Templates over the owner type

`State<T>` and `StateMachine<S>` are written once for enemies and weapons;
the trait `StateType<T>` supplies each owner's enum (see
[State machines with templates](state-machines.md)).

### Small generic helpers

- `RecordWriter::Line<Number>(key, value)` writes any number type with
  `std::to_chars`.
- `TextureManager::MaskOf<Solid>(width, height, solid)` builds a bit mask
  from any predicate on pixels, used both for loaded images and for masks
  described by test strings.
- `FixedText<Capacity>` is sized at compile time per use: 16 characters
  for a percentage, 96 for the saved game's description.

## Pitfalls

- Templates live in headers, so a change recompiles every user; the
  engine keeps them small.
- A constrained template whose constraint is too loose still fails deep
  inside the body; `std::predicate` states the whole requirement of
  `SetGrid`'s callback.
