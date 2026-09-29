# Special members and pinned types

## The technique

A C++ class has six **special member functions** the compiler can write
for it: the default constructor, the destructor, the copy constructor and
copy assignment, the move constructor and move assignment. The rules for
when they are generated are subtle: declaring a destructor or a copy
operation, for example, quietly suppresses the implicit moves, so a class
that "moves" actually copies.

Two guidelines tame this:

- **The Rule of Zero**: a class that owns nothing directly (its members
  manage themselves: `std::vector`, `std::unique_ptr`, other such classes)
  declares none of the six and gets correct copies and moves for free.
- **The Rule of Five**: a class that must manage something by hand declares
  all five of destructor, copies and moves, deliberately.

A third category matters in engines: types that must **never move**,
because other objects hold pointers into them. Such a type deletes its
copy and move operations; it is *pinned* to where it was built.

## Where it appears here

Commit `1a4f205` ("Apply the Rule of Zero and make copy/move semantics
explicit") went through the code base; the result is visible everywhere:

- Value types (`vector2d`, `Position2D`, `Ray`, `PlayerCommand`,
  `SavedGame`, configs) declare nothing and are cheaply copied.
- Resource owners (`RendererContext`, `TextureManager`, `SoundManager`,
  `ui::Ui`) delete copy and move: "a copy would destroy them twice".
- Objects that are pointed into (`Player`, `Enemy`, `Weapon`,
  `StateMachine`, `Scene`, `World`, `ObjectPool`, `MonotonicArena`)
  delete copy and move. Each comment says what points in: "Pinned: its
  weapons' states point back to the weapons inside it".
- Interfaces (`IGameObject`, `ICharacter`, `IRenderer`, `State<T>`) make
  their copy and move operations **protected**:

```cpp title="src/Characters/include/Characters/character.h"
class ICharacter
{
  public:
    virtual ~ICharacter() = default;

  protected:
    // Copies and moves only through derived classes: copying through the
    // base would slice off the derived part
    ICharacter() = default;
    ICharacter(const ICharacter&) = default;
    ICharacter& operator=(const ICharacter&) = default;
    ICharacter(ICharacter&&) = default;
    ICharacter& operator=(ICharacter&&) = default;

  public:
    virtual void SetPosition(const Position2D position) = 0;
    virtual const Position2D& GetPosition() const = 0;
    virtual void IncreaseHealth(double amount) = 0;
    virtual void DecreaseHealth(double amount) = 0;
    virtual double GetHealth() const = 0;
};
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/Characters/include/Characters/character.h#L59-L79){ .excerpt-source }

Copying an object through a reference to its base class copies only the
base part (**slicing**). Protected copies let derived classes use them but
stop `ICharacter copy = someEnemy;`.

## Guarantees checked at compile time

The rules are enforced by a test that is mostly `static_assert`s, so a
regression is a build error:

```cpp title="tests/type_traits_test.cpp"
template <typename T>
constexpr bool kPinned =
    !std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T> &&
    !std::is_move_constructible_v<T> && !std::is_move_assignable_v<T>;

// std::vector relocates elements by move only if the move cannot throw;
// otherwise every reallocation copies them
static_assert(std::is_nothrow_move_constructible_v<Ray>);
static_assert(std::is_nothrow_move_assignable_v<Ray>);
static_assert(std::is_nothrow_move_constructible_v<StaticObject>);
static_assert(std::is_nothrow_move_constructible_v<DynamicObject>);

// Plain data: copying is a memcpy
static_assert(std::is_trivially_copyable_v<Position2D>);

// Owners of SDL resources: a copy would free them twice
static_assert(kPinned<RendererContext>);
static_assert(kPinned<ui::Ui>);
static_assert(kPinned<Game>);

// Their states hold a pointer back to them
static_assert(kPinned<Weapon>);
static_assert(kPinned<Enemy>);

// Interfaces cannot be copied through the base (it would slice)
static_assert(!std::is_copy_constructible_v<IGameObject>);
static_assert(!std::is_copy_constructible_v<ICharacter>);
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/tests/type_traits_test.cpp#L21-L47){ .excerpt-source }

The `noexcept` moves matter: `std::vector` moves its elements when it
grows only if their move constructor cannot throw; otherwise it copies
them.

## Living with pinned objects

A pinned object must be built where it will live:

- `std::optional<T>::emplace` builds in place (the `World`'s player and
  scene);
- pools build in their slots with `std::construct_at` (enemies);
- `std::unique_ptr` puts it on the heap and moves the *pointer* (the
  `Game`'s subsystems);
- weapons are built in place in an array of `std::optional<Weapon>`
  inside the player.

## Pitfalls

- A defaulted `operator=` in a class with reference members is implicitly
  deleted; the engine uses pointers where a member must be reseated.
- Declaring `~T() = default;` in a class is harmless for copies but still
  suppresses implicit moves; pinned types delete them explicitly anyway.
