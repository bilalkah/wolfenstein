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

Digital audio is a stream of samples, here 32-bit floats, two per frame
(left and right), 44,100 frames a second. A **mixer** adds several
sounds' samples together into the stream the device plays, each scaled
by a gain. The audio device asks for more samples every few milliseconds
on a thread of its own (the audio thread), through a callback.

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

The game decides what to play on its own thread; the mix is made on the
audio thread. Protecting shared voices with a mutex would make the game
thread wait on the audio thread, and worse the other way round (an audio
callback that blocks causes an audible glitch). A **single-producer,
single-consumer ring** of commands avoids both: the game writes commands
into a fixed array and publishes a counter; the audio thread reads up to
that counter at the start of each mix and owns the voices outright.

## How it is implemented here

### One mix: the music, then the effects

`SoundManager::Open` sets the device up itself, rather than letting
SDL_mixer own it:

1. an SDL_mixer **mixer** that plays to no device (`MIX_CreateMixer`), in
   float stereo at 44.1 kHz, holding each music track the game plays on
   a track of its own;
2. the 36 sound effects, read from WAV files and converted to that format
   once (`SDL_ConvertAudioSamples`), each a `SoundClip`: its samples and
   its level;
3. last, the **device stream** (`SDL_OpenAudioDeviceStream`), with the
   game's own callback, `Feed`, which fills a fixed buffer 2048 frames at
   a time: SDL_mixer renders the music into it (`MIX_Generate`), and the
   `SpatialMixer` adds every sound effect on top. SDL converts the result
   to whatever the device plays.

```cpp title="src/SoundManager/src/sound_manager.cpp"
void SoundManager::Generate(int frames) {
    const int bytes = frames * kFrameBytes;
    if (MIX_Generate(mixer_, feed_.data(), bytes) != bytes) {
        std::ranges::fill(feed_, 0.0F);
    }
    spatial_.Mix(feed_.data(), frames, kMix.channels);
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/SoundManager/src/sound_manager.cpp#L231-L237){ .excerpt-source }

```cpp title="src/SoundManager/src/sound_manager.cpp"
void SDLCALL SoundManager::Feed(void* manager, SDL_AudioStream* stream,
                                int additional_amount, int /*total_amount*/) {
    auto& sound = *static_cast<SoundManager*>(manager);
    for (int left = additional_amount; left > 0;) {
        const int frames =
            std::min((left + kFrameBytes - 1) / kFrameBytes, kFeedFrames);
        sound.Generate(frames);
        SDL_PutAudioStreamData(stream, sound.feed_.data(),
                               frames * kFrameBytes);
        left -= frames * kFrameBytes;
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/SoundManager/src/sound_manager.cpp#L239-L250){ .excerpt-source }

Every effect goes through the `SpatialMixer`, in one of two ways:

- `PlayEffect(channel, effect)`: the player's own sounds, heard as
  recorded, from nowhere in particular. Each sound source gets a channel
  for life (`AllocateChannel`, round-robin over 16), which has one voice:
  a new sound from a source cuts off its own last one, never another's.
- `PlayAt(effect, where, muffled, source)`: a sound from a place, panned
  and faded as the listener moves and turns. A source other than 0 (an
  enemy, a door, another player's gun or feet) has one voice too.

Effects are an `enum class SoundEffect` indexing an array of clips:
playing one involves no string and no lookup. SDL_mixer's own tracks are
not used for effects: a track allocates whenever it is given a new sound,
and the game plays sounds constantly.

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
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/SoundManager/src/spatial_mixer.cpp#L20-L43){ .excerpt-source }

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
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/fab3414ad0f99c23307e2a7cb6a5b7beaadb0f61/src/SoundManager/src/spatial_mixer.cpp#L60-L68){ .excerpt-source }

Two commands exist: *play this clip from here* (or on this channel) and
*the listener is now here, facing this way* (sent every frame). The ring holds 256 commands;
the counters are free-running 32-bit integers, so `sent - taken` is the
number of commands in flight even after they wrap. The memory orders are
the textbook pairing: the release store of `sent_` publishes the command
written before it, and the audio thread's acquire load of `sent_` sees it.
See [Atomics and a lock-free ring](../techniques/lock-free-ring.md).

### The mix

`Mix` takes every command sent since the last call, then adds each of its
40 voices into the buffer:

```cpp title="src/SoundManager/src/spatial_mixer.cpp"
void SpatialMixer::Mix(float* out, int frames, int channels) {
    // What the game asked since the last mix, in order
    std::uint32_t taken = taken_.load(std::memory_order_relaxed);
    const std::uint32_t sent = sent_.load(std::memory_order_acquire);
    for (; taken != sent; ++taken) {
        Take(commands_[taken % kCommands]);
    }
    taken_.store(taken, std::memory_order_release);
    if (frames <= 0 || channels <= 0) {
        return;
    }

    const auto stride = static_cast<std::size_t>(channels);
    // Mono: left and right go into its one channel, at half each
    const std::size_t right_channel = channels > 1 ? 1 : 0;
    const float share = channels > 1 ? 1.0F : 0.5F;
    for (Voice& voice : voices_) {
        if (voice.clip == nullptr) {
            continue;
        }
        const float* in = voice.clip->samples;
        const std::uint32_t count = std::min(static_cast<std::uint32_t>(frames),
                                             voice.clip->frames - voice.at);
        // The sound's own level, and the effects' volume
        const float level =
            volume_.load(std::memory_order_relaxed) * voice.clip->level * share;
        const float left = voice.gain.left * level;
        const float right = voice.gain.right * level;
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t from = 2 * static_cast<std::size_t>(voice.at + i);
            float* frame = out + stride * i;
            if (voice.placed) {
                // Heard as one voice, from its place: both its channels
                // together
                const float sample = 0.5F * (in[from] + in[from + 1]);
                frame[0] += sample * left;
                frame[right_channel] += sample * right;
            }
            else {
                frame[0] += in[from] * left;
                frame[right_channel] += in[from + 1] * right;
            }
        }
        voice.at += count;
        if (voice.at >= voice.clip->frames) {
            voice = Voice{};
        }
    }
}
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/60a190225b296a849c4d27c806ea9186e85a324b/src/SoundManager/src/spatial_mixer.cpp#L114-L162){ .excerpt-source }

A voice from a place is heard as one point source: its stereo sample is
averaged, then split between the ears by the voice's gains. A voice's
gains are recomputed whenever the listener moves or turns, so a sound
already playing pans as the player turns. A channel's voice plays both
its channels as recorded. When every voice is busy, the quietest gives
way. The mix is not clamped here: it is float, and what goes past full
scale is clipped further on, where the device's format needs it.

### Music

Every track the game plays (the menu's and each level's) is loaded at
startup (decoded as it plays) onto a track of its own, played on loop,
fading in; asking for the track already playing leaves it alone, so
consecutive levels with the same track do not restart it. At startup each
track is played for a moment, unheard, and stopped: SDL_mixer sets up
what a track needs the first time it plays, which would otherwise happen
as a level starts. Volumes: a master level, with music and effects as
shares of it.

## Design decisions and trade-offs

- **The game mixes every effect itself.** SDL_mixer can position a
  sound, and SDL3_mixer plays sounds on tracks, but a track allocates each
  time it is given a new sound, and the zero-allocation rule forbids that.
  The spatial mixer allocates nothing; SDL_mixer does what it is best at,
  decoding and playing the music.
- **The game opens the device.** Filling the device's stream itself lets
  the game play the music and the effects into one buffer, and prime
  every track at startup, unheard.
- **Float all the way.** The clips are converted to the mix's format once,
  at startup; mixing adds floats, with no clamping per voice.
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
- **Forty voices.** Eight players firing, their footsteps and the doors
  can take them all; the quietest gives way, which is the right one to
  lose.
- **The web's audio clock.** SDL 3's web backend still drives the device
  from a `ScriptProcessorNode`, which browsers mark as deprecated and
  which runs on the main thread; see [Audio in the browser](../web/audio.md).

## Possible improvements

- Occlusion by path length (the noise flood fill already computes it)
  instead of a line-of-sight yes or no.
- A low-pass filter for muffled sounds, not just a lower level.
- Doppler for rockets; the projectile's velocity is known.
