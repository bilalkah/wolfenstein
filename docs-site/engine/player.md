# The player

## Purpose

`Player` is the one character the human controls. It holds everything that
travels with the player from level to level (health, the weapons carried
and their rounds) and everything about being in a level (pose, keys, the
view's pitch, a shot's kick, the fall when killed). It takes a
`PlayerCommand` each tick and acts on it; it never reads an input device.

Code: `src/Characters/include/Characters/player.h`,
`src/Characters/src/player.cpp`.

## Concepts

### Commands in, state out

Separating *what the player wants* (a command) from *what the player is*
(state) is the basis of deterministic simulation, replays and networked
games: the same commands applied to the same state give the same result,
wherever they come from. The player here is a pure consumer of commands;
the `Game` produces them (see [Input](input.md)).

## How it is implemented here

### What the player holds

| State | Notes |
| --- | --- |
| Pose and facing | `position_`, and `previous_position_` for drawing between ticks |
| Pitch | How far up or down the view is tipped, a share of the screen's height, clamped to \(\pm 0.4\) (`kMaxPitch`) |
| Health | 0 to 100 |
| Weapons | Every weapon of the configuration's arsenal (seven) is built with the player, in place, in `std::array<std::optional<Weapon>, 8>`; a bit set says which are *carried* |
| In hand | `held_`, and `coming_`: the weapon to raise once the one in hand is lowered |
| Keys | A bit set of `KeyColour`; each level's keys open its own doors, so a level starts with none |
| Feedback | The damage overlay's and the pickup flash's fades, the hit marker, the kick |

### A tick of the player

`Player::Update` switches weapons, fires or reloads, advances the weapon's
state machine, swaps in the next weapon once the last is down, then moves
and turns (see the note on this order in
[One frame](../architecture/frame-lifecycle.md#pitfalls-in-this-order)).
Firing asks the weapon; a hitscan weapon's shot is resolved at once
(`ResolvePlayerShot`), a projectile weapon launches a rocket or bolt, and
every shot makes a noise enemies can hear (`Scene::MakeNoise`).

### Taking pickups

```cpp title="src/Characters/src/player.cpp"
bool Player::TryPickUp(const PickupEffect& effect, double supplies) {
    bool taken = false;
    if (effect.health > 0.0 && health_ < 100.0) {
        IncreaseHealth(effect.health * supplies);
        taken = true;
    }
    // An ammo box tops up every firearm carried
    if (effect.ammo_boxes > 0) {
        for (std::size_t i = 0; i < weapon_count_; ++i) {
            if (Owns(i) &&
                GetWeapon(i).AddAmmoBoxes(effect.ammo_boxes,
                                          supplies * effect.box_share)) {
                taken = true;
            }
        }
    }
    // A weapon found is taken in hand; one already carried gives a box of
    // its rounds instead
    for (std::size_t i = 0; i < weapon_count_; ++i) {
        if ((effect.weapons >> i & 1U) == 0) {
            continue;
        }
        if (!Owns(i)) {
            owned_ |= static_cast<std::uint8_t>(1U << i);
            SelectWeapon(i);
            taken = true;
        }
        else if (GetWeapon(i).AddAmmoBoxes(1, supplies)) {
            taken = true;
        }
    }
    if ((effect.keys & ~keys_) != 0) {
        keys_ |= effect.keys;
        taken = true;
    }
    if (taken) {
        // What it sounds like, by the most it gave: a gun, a key, rounds,
        // else health
        const SoundEffect sound =
            effect.weapons != 0 ? SoundEffect::WeaponPickup
            : effect.keys != 0    ? SoundEffect::KeyPickup
            : effect.ammo_boxes > 0 && effect.health <= 0.0
                ? SoundEffect::AmmoPickup
                : SoundEffect::Pickup;
        sound_.PlayEffect(sound_channel_, sound);
        picked_up_ = true;
        pickup_animation_.Reset();
    }
    return taken;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/player.cpp#L196-L245){ .excerpt-source }

The rule is "take it only if it does something": a medkit at full health
and an ammo box with every reserve full stay on the floor for later.
`supplies` is the difficulty's multiplier (more on Easy, less on Hard).

### Switching weapons

A weapon is never swapped instantly. `SelectWeapon` marks the new one as
coming and puts the one in hand into its `Lowering` state; when it is down,
the next is taken and `Raising` plays; changing one's mind while the gun
goes down raises the other one, or brings the same one back up. The HUD
shows the chosen slot at once (`ComingWeapon()`), while the animation
plays. Number keys and the mouse wheel feed `PlayerCommand::weapon` and
`cycle`, each applied once.

### Being hurt, and dying

```cpp title="src/Characters/src/player.cpp"
void Player::DecreaseHealth(double amount) {
    if (!is_alive_) {
        return;     // fallen: nothing more hurts it
    }
    health_ -= amount;
    if (health_ <= 0.0) {
        is_alive_ = false;
    }
    sound_.PlayEffect(sound_channel_, SoundEffect::PlayerPain);
    since_hurt_ = 0.0;
    damaged_ = true;
    damage_animation_.Reset();
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/player.cpp#L163-L175){ .excerpt-source }

A dead player falls for 0.9 s. The fall eases in (\(t^2\), as things fall)
and drives three things: the eye drops from half a wall to near the floor,
the view rolls up to 80 degrees onto its left side, and the weapon drops
out of the picture. A thud plays as the body lands; the game-over screen
follows two seconds later.

```cpp title="src/Characters/src/player.cpp"
double Player::GetDeathFall() const {
    if (is_alive_) {
        return 0.0;
    }
    const double t = std::min(since_death_ / kFallSeconds, 1.0);
    return t * t;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/player.cpp#L257-L263){ .excerpt-source }

```cpp title="src/Characters/src/player.cpp"
double Player::GetEyeHeight() const {
    // Half a wall up, down to the floor but for a head's height
    constexpr double kStanding = 0.5;
    constexpr double kLying = 0.08;
    return kStanding - (kStanding - kLying) * GetDeathFall();
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Characters/src/player.cpp#L265-L270){ .excerpt-source }

### The kick

Each shot jolts the view up by the weapon's `kick` (a share of the screen's
height), which settles back exponentially within about a tenth of a
second (`kick_ *= exp(-30 * dt)` per tick). The renderer draws it
interpolated between ticks (`GetRenderKick(alpha)`), so it eases smoothly
at any frame rate.

## Design decisions and trade-offs

- **Every weapon exists from the start.** The arsenal's weapons (seven
  today; room for eight) are built in place with the player; "picking one up" flips a bit. No allocation when
  a weapon is found, and a saved game restores weapons by index.
- **Keys are per level.** A level's locks and keys are designed together
  (the level-design tests check that every key can be reached without
  going through its own door).
- **The player is not a level object.** It is an `IGameObject`, but not in
  the level's object list: it outlives levels, and the scene updates it
  separately after the objects.

## Pitfalls

- **Order inside `Update`.** Shooting and moving happen before `Rotate()`
  takes the command's view, so both use the previous tick's facing; see
  [One frame](../architecture/frame-lifecycle.md#pitfalls-in-this-order).
- **Weapons are indexed by position in `config.json`.** Saved games store
  weapon indices; reordering the arsenal changes what a save restores.
  `SavedGame::kFormat` exists to reject saves from before such changes.

## Possible improvements

- Move `Rotate()` to the start of `Update` (and test a shot fired in the
  same tick as a flick of the mouse).
- Armour, as in *Doom*, would be one more field and one more
  `PickupEffect` member.
