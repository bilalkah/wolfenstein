# Hearing where sounds are

| | |
| --- | --- |
| **When** | 28 and 29 September 2026 |
| **Commits** | `72cdfdb` Ft sound manager (2024), `7524277` Give doors, enemies, footsteps and each pickup a sound, `4f55a94` enemy voices, `d41e706` Hear sounds from where they are |
| **Code today** | [Audio](../engine/audio.md) |

## Problem

"Every sound played at full volume from the middle, whoever made it and
wherever: an enemy dying across the level sounded as close as one at
arm's length" (`d41e706`).

## Constraints

- No allocation per sound played.
- The game thread must never wait for the audio thread, nor the reverse.
- Walls should matter.

## Approach

From `d41e706`: "the enemies' voices and shots, the doors and the bursts
are heard from their place: from the side they come from, quieter the
further off (silent a level's width away), and muffled through a wall.
They are panned afresh every frame as the player turns. The player's own
gun, steps and cries stay as they were. SDL_mixer's own positioning
allocates for every sound played, so these are mixed in by the game, after
SDL_mixer's channels".

The `SpatialMixer` keeps 24 voices (40 since the move to SDL 3, when the
player's own sounds joined them; see [Moving to SDL 3](sdl3.md)) on the audio thread and takes commands
from the game through a lock-free single-producer, single-consumer ring.

## C++ techniques used

- [Atomics and a lock-free ring](../techniques/lock-free-ring.md)
- [Strong types](../techniques/strong-types.md): `SoundEffect` and
  `SoundChannel`.

## Key code

- [`Hear`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/SoundManager/src/spatial_mixer.cpp#L20-L43)
- [`SpatialMixer::Send`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/SoundManager/src/spatial_mixer.cpp#L60-L68)
- [`SpatialMixer::Mix`](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/SoundManager/src/spatial_mixer.cpp#L111-L158)

## Pitfalls

- Muffling is a yes-or-no from a line of sight, so a sound round a corner
  and one behind a thick wall are muffled alike.
- Positional sound needs the device in 16-bit stereo; otherwise the sounds
  play centred.

## What I'd change

- Muffle by the path length through open cells (the noise flood fill
  already computes it), with a low-pass filter.
