# Compile-time code and attributes

## The technique

- `constexpr` variables are constants the compiler knows; `constexpr`
  functions may be evaluated at compile time when their arguments are.
- `static_assert(condition, message)` stops the build when a condition on
  types or constants fails.
- Attributes give the compiler facts it cannot infer: `[[nodiscard]]`
  (warn if a return value is ignored), `[[maybe_unused]]` (silence an
  unused warning deliberately), `[[unlikely]]` (a branch is cold).
- `std::unreachable()` (C++23) marks a point control never reaches;
  reaching it is undefined behaviour, so the optimiser may assume it away.
- `<numbers>` (C++20) gives typed constants such as `std::numbers::pi`.

## Where it appears here

### Named constants, everywhere

About 220 `constexpr` in `src/` and `app/`, almost all named tuning
constants next to the code that uses them, with a comment on what they
mean: `kCollisionDistance`, `kDoorMoveSeconds`, `kReadReach`,
`kCrowdCost`, `kWarmUpCommands`... Magic numbers are rare; a value with a
name and a reason is easy to tune and to find.

Some functions are `constexpr` too, so they can be used in constant
expressions: `Map::IsDoorCell`, `KeyBit`, `ToIndex`.

### Checks the build enforces

- The sound effect count matches the enum (see
  [Strong types](strong-types.md)).
- The renderer has room for every decal it may draw:
  `static_assert(kDecals >= Scene::kWallMarks + 4 + Scene::kIntel, ...)`.
- Copy and move guarantees of core types (`tests/type_traits_test.cpp`,
  see [Special members](special-members.md)).

### Attributes

- `[[nodiscard]]` on `ObjectPool::Create`: dropping a `Handle` (or an
  error) is almost certainly a bug.
- `[[unlikely]]` on the arena's out-of-memory branches, which should never
  run.
- `[[maybe_unused]]` on the AddressSanitizer poisoning helpers' parameters,
  unused when the sanitizer is off.
- `std::unreachable()` after exhaustive `switch`es over `enum class`es
  (`Enemy::StateFor`, `Weapon::StateFor`), and where a `std::optional`
  weapon cannot be empty.

### Compile-time format strings

`std::format_string<Args...>` (C++20) checks a format string against its
arguments at compile time: `FixedText<32> text("{} / {}", kills, total)`
fails to compile if the placeholders do not match. See
[Text without allocating](allocation-free-text.md).

## Pitfalls

- `std::unreachable()` is a promise, not a check: if a new enumerator is
  added and not handled, `-Wswitch` warns (and `-Werror` makes it an
  error), which is what keeps the promise true.
- `constexpr` on a function does not force compile-time evaluation; use
  `consteval` for that (the code has no need of it).
