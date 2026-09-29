# Audio

## Purpose

The audio subsystem plays the game's 36 sound effects and its music: the
player's gun and footsteps, as heard from nowhere in particular, and every
sound from a place in the level (an enemy's shot, a door, a rocket
bursting) **from where it is**: to the left or right, quieter further
off, dull through a wall. It must do this without allocating while the
game runs, and without the game thread and the audio thread ever waiting
for each other.

Code: `src/SoundManager/` (`sound_manager.cpp`, `spatial_mixer.cpp`).

## Concepts

### Mixing

Digital audio is a stream of samples, here 16-bit signed integers, two
per frame (left and right). A **mixer** adds several sounds' samples
together into the stream the device plays, each scaled by a gain, clamping
the sum to the 16-bit range. SDL_mixer does this for its channels on an
audio thread, calling back for more samples every few milliseconds.

### Positional sound in two dimensions

With the listener at \(E\) facing \(\theta\) and a source at \(S\), at
distance \(d\) and bearing \(\phi\):

- **Loudness** falls with distance. Here it is full within 2 cells,
  silent from 24, and falls faster near than far:
  \(g = r^2\) with \(r = \operatorname{clamp}\left(\frac{24 - d}{24 - 2}, 0, 1\right)\).
  Through a wall (no line of sight) it is multiplied by 0.45.
- **Panning** follows the bearing relative to the facing:
  \(s = 0.8 \sin(\phi - \theta)\), from \(-0.8\) (left) to \(0.8\)
  (right). The ear on the far side still hears a fifth of it (never
  silent), and a sound straight behind is heard in both ears alike:
  \(g_{left} = g \min(1, 1 - s)\), \(g_{right} = g \min(1, 1 + s)\).

### Talking to the audio thread without locks

The game decides what to play on its own thread; SDL_mixer mixes on the
audio thread. Protecting shared voices with a mutex would make the game
thread wait on the audio thread, and worse the other way round (an audio
callback that blocks causes an audible glitch). A **single-producer,
single-consumer ring** of commands avoids both: the game writes commands
into a fixed array and publishes a counter; the audio thread reads up to
that counter at the start of each mix and owns the voices outright.

## How it is implemented here

### Two paths

- `PlayEffect(channel, effect)`: the player's own sounds, through
  SDL_mixer's channels. Each sound source gets a channel for life
  (`AllocateChannel`, round-robin over 16), so a new sound from a source
  cuts off its own last one, never another's.
- `PlayAt(effect, where, muffled, source)`: a sound from a place, through
  the `SpatialMixer`, used when the device plays 16-bit stereo (as it is
  asked to); otherwise it falls back to a plain channel.

Effects are an `enum class SoundEffect` indexing an array of loaded
`Mix_Chunk*`: playing one involves no string and no lookup ("a string name
allocated on wasm32, whose short-string buffer holds only 10 characters",
the comment on `PlayEffect` notes).

### Hearing a sound from a place

```cpp title="src/SoundManager/src/spatial_mixer.cpp"
StereoGain Hear(const vector2d& ear, double theta, const vector2d& source,
                bool muffled) {
    // Full up to kNear cells away, silent from kFar, falling off faster
    // near than far, as loudness does
    constexpr double kNear = 2.0;
    constexpr double kFar = 24.0;
    constexpr double kThroughWall = 0.45;
    // A sound from one side is still a little heard in the other ear
    constexpr double kWidest = 0.8;
    const vector2d to = source - ear;
    const double distance = to.Magnitude();
    const double reach =
        std::clamp((kFar - distance) / (kFar - kNear), 0.0, 1.0);
    double loud = reach * reach;
    if (muffled) {
        loud *= kThroughWall;
    }
    // +1 to the right (the way a player strafes right), -1 to the left
    const double side =
        distance < 1e-6 ? 0.0
                        : kWidest * std::sin(std::atan2(to.y, to.x) - theta);
    return {.left = static_cast<float>(loud * std::min(1.0, 1.0 - side)),
            .right = static_cast<float>(loud * std::min(1.0, 1.0 + side))};
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/SoundManager/src/spatial_mixer.cpp#L20-L43){ .excerpt-source }

Whether a sound is muffled is decided by the `Scene`
(`PlaySoundAt`): muffled if there is no line of sight from the player to
the source, unless the source is in a blocked cell itself (a door sliding
in its frame is not *behind* the door).

### The command ring

```cpp title="src/SoundManager/src/spatial_mixer.cpp"
bool SpatialMixer::Send(const Command& command) {
    const std::uint32_t sent = sent_.load(std::memory_order_relaxed);
    if (sent - taken_.load(std::memory_order_acquire) == kCommands) {
        return false;
    }
    commands_[sent % kCommands] = command;
    sent_.store(sent + 1, std::memory_order_release);
    return true;
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/SoundManager/src/spatial_mixer.cpp#L60-L68){ .excerpt-source }

Two commands exist: *play this chunk from here* and *the listener is now
here, facing this way* (sent every frame). The ring holds 256 commands;
the counters are free-running 32-bit integers, so `sent - taken` is the
number of commands in flight even after they wrap. The memory orders are
the textbook pairing: the release store of `sent_` publishes the command
written before it, and the audio thread's acquire load of `sent_` sees it.
See [Atomics and a lock-free ring](../techniques/lock-free-ring.md).

### The mix

On the audio thread, SDL_mixer calls `SpatialMixer::MixHook` after mixing
its own channels (`Mix_SetPostMix`). `Mix` takes every command sent since
the last call, then adds each of its 24 voices into the stream:

```cpp title="src/SoundManager/src/spatial_mixer.cpp"
void SpatialMixer::Mix(std::uint8_t* stream, int bytes) {
    // What the game asked since the last mix, in order
    std::uint32_t taken = taken_.load(std::memory_order_relaxed);
    const std::uint32_t sent = sent_.load(std::memory_order_acquire);
    for (; taken != sent; ++taken) {
        Take(commands_[taken % kCommands]);
    }
    taken_.store(taken, std::memory_order_release);

    auto* out = reinterpret_cast<std::int16_t*>(stream);
    const auto frames = static_cast<std::uint32_t>(bytes) / kFrameBytes;
    constexpr float kLowest = std::numeric_limits<std::int16_t>::min();
    constexpr float kHighest = std::numeric_limits<std::int16_t>::max();
    for (Voice& voice : voices_) {
        if (voice.chunk == nullptr) {
            continue;
        }
        const auto* in =
            reinterpret_cast<const std::int16_t*>(voice.chunk->abuf);
        const std::uint32_t count =
            std::min(frames, FramesOf(*voice.chunk) - voice.at);
        // The sound's own level, as SDL_mixer would play it, and the
        // effects' volume
        const float level = volume_.load(std::memory_order_relaxed) *
                            static_cast<float>(voice.chunk->volume) /
                            static_cast<float>(MIX_MAX_VOLUME);
        const float left = voice.gain.left * level;
        const float right = voice.gain.right * level;
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t from = 2 * static_cast<std::size_t>(voice.at + i);
            // Heard as one voice, from its place: both its channels together
            const float sample = 0.5F * (static_cast<float>(in[from]) +
                                         static_cast<float>(in[from + 1]));
            std::int16_t& out_left = out[2 * static_cast<std::size_t>(i)];
            std::int16_t& out_right = out[2 * static_cast<std::size_t>(i) + 1];
            out_left = static_cast<std::int16_t>(
                std::clamp(static_cast<float>(out_left) + sample * left,
                           kLowest, kHighest));
            out_right = static_cast<std::int16_t>(
                std::clamp(static_cast<float>(out_right) + sample * right,
                           kLowest, kHighest));
        }
        voice.at += count;
        if (voice.at >= FramesOf(*voice.chunk)) {
            voice = Voice{};
        }
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/src/SoundManager/src/spatial_mixer.cpp#L111-L158){ .excerpt-source }

Each voice is heard as one point source: its stereo sample is averaged,
then split between the ears by the voice's gains. A voice's gains are
recomputed whenever the listener moves or turns, so a sound already
playing pans as the player turns. When all 24 voices are busy, the
quietest gives way; a source other than 0 (an enemy) has one voice, and
its new sound replaces its last.

### Music

Every track the game plays (the menu's and each level's) is loaded at
startup as a `Mix_Music` and played on loop, fading in; asking for the
track already playing leaves it alone, so consecutive levels with the same
track do not restart it. Volumes: a master level, with music and effects
as shares of it.

## Design decisions and trade-offs

- **A custom spatial mixer instead of `Mix_SetPosition`.** SDL_mixer can
  position a channel, but registers an effect per play that allocates, and
  channels are removed from positioning when they finish; the game plays
  sounds constantly, and the zero-allocation rule forbids that (the
  comment on `SpatialMixer`: "SDL_mixer's own positioning (an effect per
  channel) allocates every time a sound starts; this allocates nothing").
- **Loudness squared.** Loudness perceived falls off faster up close than
  far; \(r^2\) is a cheap approximation of that curve.
- **Stereo, not HRTF.** Left and right panning only; behind and in front
  sound the same. Good enough to tell where a shot came from on a screen
  you face.
- **Lock-free, not locked.** The audio thread never waits for the game.

## Pitfalls

- **A full ring drops commands.** `Send` returns false when 256 commands
  are waiting; the game does not retry. Between two mixes (a few
  milliseconds) the game sends far fewer.
- **Only 16-bit stereo is mixed spatially.** The device is asked for
  `MIX_DEFAULT_FORMAT` in stereo; if a platform opens something else, the
  positional sounds fall back to plain channels, centred.
- **The web's audio clock.** SDL2's web backend drives the mixer from a
  `ScriptProcessorNode`, which browsers mark as deprecated (the console
  says so on start) and which runs on the main thread; see
  [Audio in the browser](../web/audio.md).

## Possible improvements

- Occlusion by path length (the noise flood fill already computes it)
  instead of a line-of-sight yes or no.
- A low-pass filter for muffled sounds, not just a lower level.
- Doppler for rockets; the projectile's velocity is known.
