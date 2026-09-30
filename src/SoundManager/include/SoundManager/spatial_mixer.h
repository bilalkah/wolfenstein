/**
 * @file spatial_mixer.h
 * @brief The sound effects' mix: panned, faded with distance, muffled
 */

#ifndef SOUND_MANAGER_INCLUDE_SOUND_MANAGER_SPATIAL_MIXER_H
#define SOUND_MANAGER_INCLUDE_SOUND_MANAGER_SPATIAL_MIXER_H

#include "Math/vector.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace karakale {

// How loud a sound is in each ear, 0 to 1
struct StereoGain
{
	float left = 0.0F;
	float right = 0.0F;
};

// How loud a sound from `source` is in each ear of a listener at `ear`
// facing `theta`: full within a couple of cells, fading to nothing a level's
// width away; to the side it comes from (sin of its angle from ahead:
// straight ahead and straight behind alike in both ears), never quite
// silent in the other ear; and quieter still through a wall (`muffled`)
StereoGain Hear(const vector2d& ear, double theta, const vector2d& source,
				bool muffled);

// A sound effect as the mixer plays it: 32-bit float stereo samples at the
// mix's rate, and how loud it plays (1 as recorded)
struct SoundClip
{
	const float* samples = nullptr;	 // left, right, left, right, ...
	std::uint32_t frames = 0;
	float level = 1.0F;
};

// Plays the sound effects on a fixed set of voices, mixed in over the music
// SDL_mixer has mixed (see SoundManager). A sound from a place in the level
// is panned and faded as the listener moves and turns; a channel's sound
// (the player's own) is heard from nowhere in particular. SDL_mixer's own
// tracks allocate every time one is given a new sound to play; this
// allocates nothing.
//
// The game's thread asks (Play, PlayCentred, SetListener) and the audio
// thread mixes (Mix). The asking goes over a fixed ring of commands, one
// writer and one reader, so neither waits for the other: the audio thread
// owns the voices and takes the commands as it starts each mix.
class SpatialMixer
{
  public:
	// The 16 channels the player's own sounds had and the 24 voices sounds
	// from places had, before both were mixed here
	static constexpr std::size_t kVoices = 40;

	// The game's thread: plays `clip` as heard from `where`, muffled through
	// a wall or not. A `source` other than 0 (an enemy's) has one voice: its
	// new sound cuts off its last. With every voice busy, the quietest gives
	// way.
	void Play(const SoundClip* clip, const vector2d& where, bool muffled,
			  std::uint32_t source);
	// The game's thread: plays `clip` in both ears as recorded, wherever the
	// listener is, on `channel`'s one voice: its new sound cuts off its last
	void PlayCentred(const SoundClip* clip, std::uint32_t channel);
	// The game's thread: where the listener is and faces; every voice from a
	// place is heard afresh from there
	void SetListener(const vector2d& ear, double theta);
	// The effects' volume, 0 to 1
	void SetVolume(float volume) {
		volume_.store(volume, std::memory_order_relaxed);
	}

	// The audio thread: takes what was asked, then adds the voices into
	// `out`, `frames` sample frames of `channels` samples each (left and
	// right first), moving each on; a voice that reaches its end is free
	// again
	void Mix(float* out, int frames, int channels);
	// The audio thread (or a test, mixing itself): voices playing
	std::size_t Playing() const;

  private:
	struct Command
	{
		enum class Kind : std::uint8_t { Play, Listen };
		Kind kind = Kind::Play;
		const SoundClip* clip = nullptr;
		vector2d where{};  // the sound's place, or the listener's
		double theta = 0.0;
		bool muffled = false;
		bool placed = true;	 // from a place, or a channel's
		std::uint32_t source = 0;
	};
	struct Voice
	{
		const SoundClip* clip = nullptr;
		std::uint32_t at = 0;  // in sample frames
		vector2d where{};
		bool muffled = false;
		bool placed = true;
		std::uint32_t source = 0;  // or the channel
		StereoGain gain{};
	};

	// The game's thread: false if the ring is full (the command is lost)
	bool Send(const Command& command);
	// The audio thread
	void Take(const Command& command);

	// A power of two: many frames' commands between two mixes
	static constexpr std::uint32_t kCommands = 256;
	std::array<Command, kCommands> commands_{};
	std::atomic<std::uint32_t> sent_{0};
	std::atomic<std::uint32_t> taken_{0};
	std::atomic<float> volume_{1.0F};

	// The audio thread's own
	std::array<Voice, kVoices> voices_{};
	vector2d ear_{};
	double theta_ = 0.0;
};

}  // namespace karakale

#endif	// SOUND_MANAGER_INCLUDE_SOUND_MANAGER_SPATIAL_MIXER_H
