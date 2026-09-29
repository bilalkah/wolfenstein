# Weapons

| | |
| --- | --- |
| **When** | August 2024 to September 2026 |
| **Commits** | `95276c9` Add shotgun with animation, `c15aade` weapon animations, `f79930a` Improve weapon state transition, `0bf0e6e` Carry an arsenal, `20c414c` rebalance the arsenal, `d8acce1` hand-drawn pistol, `d8acce1` Drop the knife, `d8acce1` Bring a weapon up, `d8acce1` Weaken the pistol, `e024275`, `7524277` Put the gun in hand away before the next comes up, `64ee770` each gun its own sound, `4f55a94` double-barrelled shotgun and saw, `4f55a94` held-down weapon |
| **Code today** | [Weapons and combat](../engine/combat.md), [The player](../engine/player.md#switching-weapons) |

## Problem

Give the player a set of weapons that feel different (rate of fire,
spread, reach, reload, sound, kick) and switching between them that looks
and sounds right, with the rounds each carries saved with the game.

## Constraints

- Weapons are data: adding one should not need code (see
  [Content as data](data-driven-content.md)).
- Animations and timing driven by states, not ad-hoc flags.
- No allocation when firing, reloading or switching.
- Saved games restore weapons by index, so the slot order must stay stable.

## Approach

- **A state machine per weapon** (`f79930a`): loaded, out of ammo,
  reloading, and later raising and lowering. "Introduce transition request
  and interrupt; introduce OnContextSet to get context values."
- **An arsenal** (`0bf0e6e`): the player carries several weapons at once,
  "each keeps its own rounds, an ammo box tops up every firearm carried".
  (The knife it started with was dropped in `d8acce1`; the saw became the
  melee weapon later.)
- **Raise and lower** (`d8acce1`, `7524277`): switching plays the gun going
  down, then the next coming up; no rounds move and a reload asked for
  meanwhile is not taken.
- **Feel**: a kick per weapon (`20c414c`), a sound per gun and a dry click
  when empty (`64ee770`), the saw's firing frames held while the trigger is
  (`4f55a94`: "The saw jumped from side to side while cutting: each stroke
  began on the picture of it at rest").
- **More weapons in more slots** (`4f55a94`, `4f55a94`): "in slots 4 and 5
  after the three there were, so saved games keep their weapons".

## C++ techniques used

- [State machines with templates](../techniques/state-machines.md) with the
  trait `StateType<Weapon>`.
- `std::optional` members for weapons built in place inside the player,
  and for optional sounds (`std::optional<SoundEffect>`).
- [Special members and pinned types](../techniques/special-members.md): a
  `Weapon` is pinned, its states point back to it.

## Key code

- [`WeaponConfig`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Strike/include/Strike/weapon.h):
  everything a weapon is, read from `config.json`.
- [`Player::SelectWeapon`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/player.cpp#L74-L95):
  lowering one weapon before raising the next, and changing one's mind.
- [`LoadedState::PullTrigger`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/State/src/weapon_state.cpp#L73-L84):
  one shot per shot's duration, the magazine decremented, the sound played.

## Pitfalls

- **Slot order is part of the save format.** New weapons went into new
  slots; reordering the arsenal would need a `SavedGame::kFormat` bump.
- **Frame 0 is "held".** A weapon's `loaded` clip starts with the gun at
  rest; a shot plays the rest, which is what fixed the saw's jitter.
- **Optional art.** Raise clips are optional (`FindClip`); the MP5's raise
  plays the end of its reload backwards.

## What I'd change

- Recoil that accumulates while firing (spread growing with the burst).
- An alternative fire per weapon, if the design ever wants one.
