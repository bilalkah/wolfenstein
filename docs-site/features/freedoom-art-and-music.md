# Freedoom art and music

| | |
| --- | --- |
| **When** | 28 September 2026 |
| **Commits** | `9b20b84` Take the art and sounds from Freedoom, with enemies seen from all 8 sides, `9b20b84` Play Freedoom's music, `4f55a94` Add a shotgun zombie, a minigun zombie and a fast demon, `7524277` doors, enemies, footsteps and pickups get sounds, `e024275` Store each animation frame once |
| **Code today** | [Textures and animation](../engine/assets.md#the-art-pipeline), `scripts/import_freedoom.py`, `assets/licenses/` |

## Problem

Much of the game's art could not be shipped. From `9b20b84`: "Much of the
art they replace was Doom's own, which cannot be shipped." And from
`9b20b84`: "theme.mp3 was the last piece of art the game could not ship."

## Constraints

- Every asset redistributable, with its licence in the repository.
- Keep the game's look (a *Doom*-like shooter) and its mechanics.
- Reproducible: art imported by a script, not copied by hand.

## Approach

- **[Freedoom](https://freedoom.github.io/)**, a free replacement for
  *Doom*'s data under a BSD licence, supplies the enemies, weapons,
  pickups, keys, torches, walls, doors, the exit, the sky, the HUD digits,
  the menu and result screens, most sounds and the music.
- `scripts/import_freedoom.py` reads `freedoom2.wad` directly (Doom's
  picture format, sprite rotations, the palette) and writes frames,
  textures, sounds, MIDI and the sizes in `config.json`.
- **Eight sides**: "An enemy is drawn from the side the viewer sees it
  from, one of 8, as the shot that meets it is tested: it faces the way it
  walks, and the player it shoots at."
- **Music** (`9b20b84`): Freedoom's intermission theme for the menu and
  its first maps' tracks for the levels, rendered from MIDI to MP3 with the
  FluidR3 soundfont (MIT licence) by `scripts/render_music.sh`.
- **The project's own art** (`scripts/make_art.py`): the crosshair, the red
  of a hit, blood and dust puffs, bullet holes, the secret crack, and
  synthesised sounds (footsteps, a dry click, a thud).

## Licensing, as the repository records it

The README's licence section and `assets/licenses/`: Freedoom's
`COPYING.txt` (BSD) and credits, the music's credits and the soundfont's
MIT licence, the fonts' licences (Black Ops One under the SIL Open Font
License, Roboto under Apache 2.0). The code is MIT.

!!! warning "The repository's history still contains the old art"
    Commits before `9b20b84` (for example `a1ae2bf`, "Add assets with git
    lfs", in August 2024) added sprites and sounds that `9b20b84`'s own
    message calls "Doom's own, which cannot be shipped". They are gone from
    the current tree, so the web build (which packs only today's
    `assets/`) does not include them, but they remain in git history and
    in Git LFS storage on GitHub.

## C++ techniques used

- None specific: the pipeline is Python. The engine side is the texture
  manifest and eight-view clips ([Textures and animation](../engine/assets.md)).

## Key code

- [`scripts/import_freedoom.py`](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/scripts/import_freedoom.py)
- [`assets/licenses/`](https://github.com/bilalkah/wolfenstein/tree/73aaf653bbc3b5dbb26be73432bb845df5e76c52/assets/licenses)

## Pitfalls

- An enemy's picture must be as wide as its widest frame, so its collision
  radius is set separately.
- Freedoom's standing frames are steps of the walk; guards stand on one.

## What I'd change

- Rewrite history to drop the non-redistributable art, if the repository's
  history matters as much as its tree (a destructive operation on a
  published repository; a decision for the author).
