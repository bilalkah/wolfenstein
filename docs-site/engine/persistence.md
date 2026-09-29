# Settings and saved games

## Purpose

Two things outlive a session: the player's **settings** (mouse
sensitivity, inverted look, field of view, volumes, the FPS counter) and
the **campaign in progress**, so a later visit goes on from about where
this one stopped. Both are small text records, kept in the browser's
`localStorage` on the web and in files natively.

Code: `src/Settings/` (`settings.cpp`, `saved_game.cpp`, `storage.cpp`),
`World::Capture` and `World::ContinueGame`, `Game::SaveProgress` and
`Game::AutoSave`.

## Concepts

### Plain, versioned records

A save file does not need to be clever: a few lines of `key=value` are
easy to write without allocating, easy to read, easy to inspect in the
browser's developer tools, and tolerant of change (a newer version's extra
keys can be skipped by an older reader). What must not happen is loading a
save written for *different content* and silently getting it wrong, so the
record carries a **format number** that is bumped whenever old saves would
be misread.

## How it is implemented here

### Where records live

```cpp title="src/Settings/src/storage.cpp"
EM_JS(char*, ReadStoredRecord, (const char* name), {
    let value = null;
    try {
        value = localStorage.getItem('wolfenstein.' + UTF8ToString(name));
    } catch (e) {
    }
    return value === null ? 0 : stringToNewUTF8(value);
});
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Settings/src/storage.cpp#L35-L42){ .excerpt-source }

- **Web:** `localStorage` keys `wolfenstein.settings` and
  `wolfenstein.progress`, reached through `EM_JS` functions (JavaScript
  bodies compiled into the module). Access is wrapped in `try`: storage
  can be blocked (some private windows), and then records read as empty
  and writes are ignored.
- **Native:** `settings.txt` and `progress.txt` in SDL's per-user
  preferences directory (`SDL_GetPrefPath("bilalkah", "wolfenstein")`).

### Writing without allocating

Saving happens while the game runs, so it must not allocate. `RecordWriter`
appends `key=value` lines into a caller's buffer with `std::to_chars`
(which never allocates, unlike `std::format` printing a `double`), and
fails cleanly if the buffer is too small. A saved game is formatted into a
1 KB array on the stack and handed to `WriteRecord`. See
[Text without allocating](../techniques/allocation-free-text.md).

### What a saved game holds

`SavedGame` is a plain struct: the campaign level, the weapon in hand, the
difficulty, the game's random seed (which rolls every enemy's drops), the
player's health, the weapons carried (a bit set) with each one's rounds in
the magazine and in reserve; and, if the player was in the middle of a
level, where they stood and what is done there: enemies killed, pickups
taken, secrets pushed and pages of intel read (64-bit bit sets indexed as
the level file orders them), keys held, the level's clock, and the cells
explored (as hex).

### The format number

```cpp title="src/Settings/include/Settings/saved_game.h"
    // Written into every save and required back: weapons are saved by their
    // index in the configuration, and levels and pickups by theirs in the
    // campaign, so a save from before either changed would give the wrong
    // ones (format 1 had a knife in the first slot; format 2, levels
    // without intel; format 3, intel lying among the pickups)
    static constexpr unsigned kFormat = 4;
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Settings/include/Settings/saved_game.h#L24-L29){ .excerpt-source }

A record with another format (or none) parses as no saved game. The game
also drops a save that points past the content it has (a level index
beyond the campaign, a weapon or difficulty that does not exist).

### When the game saves

- As every level **starts** (new game, next level).
- Every **five seconds** of play while the level is quiet: no enemy
  engaged with the player and nothing has hurt them for three seconds, so
  a save is never made in the middle of a fight:

    ```cpp title="src/Core/src/game.cpp"
    void Game::AutoSave(double delta_time) {
        constexpr double kAutoSaveSeconds = 5.0;
        since_save_ += delta_time;
        if (since_save_ >= kAutoSaveSeconds && fade_ == Fade::None &&
            !renderer_result_ &&
            world_->CurrentLevel().GetNumberOfAliveEnemies() > 0 &&
            world_->IsQuiet()) {
            SaveProgress();
        }
    }
    ```
    [View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/Core/src/game.cpp#L227-L236){ .excerpt-source }

- On **Quit to menu**, if the level is quiet.
- The save is **cleared** when the campaign is won.

Scripted runs (benchmark, soak) never touch the saved game.

### Continuing

`World::ContinueGame` starts a new game with the saved weapon and
difficulty and seed, jumps to the saved level, restores what the player
carries, and, if the save has a position, puts the level back as it was:
killed enemies lying dead (silently, straight to the end of their death),
pickups taken, secrets already slid back, intel read, the map explored,
the clock.

### Settings

`Settings` is a struct with defaults and ranges (sensitivity 0.25 to 3,
field of view 60 to 80 degrees, volumes 0 to 1). `Parse` reads
`key=value` lines, clamping values to their ranges and ignoring unknown
keys and malformed values, so an old or hand-edited file never breaks the
game. It is read once at startup and written whenever the settings screen
changes something.

## Design decisions and trade-offs

- **Text, not binary.** Slightly larger, trivially inspectable and
  forward-compatible.
- **Indices, not names.** Enemies, pickups, weapons and levels are
  recorded by index, which is compact but ties a save to the exact content;
  hence the format number.
- **Save when quiet.** Restoring enemies mid-fight (their states, routes
  and timers) would need far more state; saving only when nothing is
  engaged makes "enemies killed or not" enough.

## Pitfalls

- **Any change in a level's order of pickups or enemies** breaks old saves
  of that level: bump `kFormat`. (The intel moving from pickups to walls
  did exactly that, format 3 to 4.)
- **64 of each per level.** The bit sets are 64-bit; the level-design test
  checks no level has more than 64 enemies, or 64 pickups counting every
  possible drop.
- **localStorage is per origin.** A game served from another address (a
  LAN IP, a different port, GitHub Pages) has its own settings and saves.

## Possible improvements

- Several save slots, or a save per chapter, for replaying.
- Settings and saves exported and imported as a string, for moving between
  browsers.
