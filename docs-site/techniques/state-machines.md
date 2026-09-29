# State machines with templates

## The technique

The **State pattern** represents each state of an object as an object of
its own class, with a common interface (`OnEnter`, `Update`, ...); the
owner delegates to its current state, and a transition swaps which state
is current. A **class template** lets one implementation of that machinery
serve several owners (enemies and weapons here), each with its own set of
states. A **type trait** (a template specialised per type to carry
compile-time information) can then connect an owner type to the enum that
names its states, so generic code can ask "what kind of state is this?"
for any owner.

## Where it appears here

### The base state and the trait

`State<T>` is the base of every state of an owner `T`; its `GetType()`
returns `StateType<T>::Type`, which each owner defines by specialising the
trait:

```cpp title="src/State/include/State/enemy_state.h"
template <>
struct StateType<Enemy>
{
    enum class Type : std::uint8_t {
        Idle,
        Walk,
        Attack,
        Pain,
        Death,
        Patrol,
        Retreat
    };
};
using EnemyStateType = StateType<Enemy>::Type;
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/State/include/State/enemy_state.h#L25-L38){ .excerpt-source }

`StateType<Weapon>` is specialised the same way with `Loaded`,
`OutOfAmmo`, `Reloading`, `Raising` and `Lowering`. The primary template
is only declared (`template <typename T> struct StateType;`), so using the
trait for a type that did not specialise it is a compile error.

### The machine

```cpp title="src/State/include/State/state.h"
template <typename S>
class StateMachine
{
  public:
    StateMachine() = default;
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;
    StateMachine(StateMachine&&) = delete;
    StateMachine& operator=(StateMachine&&) = delete;
    ~StateMachine() = default;

    void TransitionTo(S& state) {
        if (updating_) {
            pending_ = &state;
            return;
        }
        Enter(state);
    }

    void Update(double delta_time) {
        updating_ = true;
        current_->Update(delta_time);
        updating_ = false;
        if (pending_ != nullptr) {
            Enter(*std::exchange(pending_, nullptr));
        }
    }

    S& Current() { return *current_; }
    const S& Current() const { return *current_; }

  private:
    void Enter(S& state) {
        current_ = &state;
        current_->OnEnter();
    }

    S* current_ = nullptr;
    S* pending_ = nullptr;
    bool updating_ = false;
};
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/State/include/State/state.h#L72-L112){ .excerpt-source }

- **States are members of their owner.** An `Enemy` holds its seven
  states by value; the machine only switches a pointer, so a transition
  allocates and frees nothing. (Until `3faf914`, every transition created
  a new state object, animation, frame list and name string.)
- **Deferred transitions.** A transition requested from inside the current
  state's `Update` takes effect after `Update` returns, so a state never
  sees itself replaced mid-update; `std::exchange` takes the pending state
  and clears it in one step.
- **Two hooks.** `OnContextSet` runs once when the owner gives the state
  its context (read configuration, build the animation); `OnEnter` runs on
  every entry (reset per-visit data, play the entry sound).

### Choosing a state by enum

```cpp title="src/Characters/src/enemy.cpp"
EnemyState& Enemy::StateFor(EnemyStateType type) {
    switch (type) {
        case EnemyStateType::Idle:
            return idle_state_;
        case EnemyStateType::Patrol:
            return patrol_state_;
        case EnemyStateType::Walk:
            return walk_state_;
        case EnemyStateType::Attack:
            return attack_state_;
        case EnemyStateType::Pain:
            return pain_state_;
        case EnemyStateType::Death:
            return death_state_;
        case EnemyStateType::Retreat:
            return retreat_state_;
    }
    std::unreachable();
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/enemy.cpp#L45-L63){ .excerpt-source }

A `switch` over an `enum class` with every case handled: the compiler warns
if a new state is added and not handled, and C++23's `std::unreachable()`
tells it the end is never reached (instead of a dummy return).

## History

The states once held a `std::shared_ptr` to their owner while the owner
held its states: a reference cycle that "leaked every owner and state".
`39846c6` replaced it with a non-owning pointer; the comment on `State<T>`
records why it is valid (the owner outlives every state it owns).

## Pitfalls

- **Owners are pinned.** States point at their owner and the machine at
  the states, all inside the owner; copying or moving it would leave those
  pointers aimed at the old object. `Enemy`, `Weapon` and `StateMachine`
  delete their copy and move operations.
- **Virtual dispatch per update.** One virtual call per enemy per tick;
  negligible here.
