/**
 * @file spatial_mixer.h
 * @brief Sounds heard from a place: panned, faded with distance, muffled
 */

#ifndef SOUND_MANAGER_INCLUDE_SOUND_MANAGER_SPATIAL_MIXER_H
#define SOUND_MANAGER_INCLUDE_SOUND_MANAGER_SPATIAL_MIXER_H

#include "Math/vector.h"
#include <SDL2/SDL_mixer.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace wolfenstein {

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

// Plays sounds from places in the level on a fixed set of voices, mixed in
// after SDL_mixer's own channels (its post-mix hook), and pans and fades each
// as the listener moves and turns. SDL_mixer's own positioning (an effect
// per channel) allocates every time a sound starts; this allocates nothing.
//
// The game's thread asks (Play, SetListener) and the audio thread mixes
// (Mix). The asking goes over a fixed ring of commands, one writer and one
// reader, so neither waits for the other: the audio thread owns the voices
// and takes the commands as it starts each mix.
class SpatialMixer
{
  public:
	static constexpr std::size_t kVoices = 24;

	// The game's thread: plays `chunk` (in the device's format: 16-bit
	// stereo) as heard from `where`, muffled through a wall or not. A
	// `source` other than 0 (an enemy's) has one voice: its new sound cuts
	// off its last. With every voice busy, the quietest gives way.
	void Play(const Mix_Chunk* chunk, const vector2d& where, bool muffled,
			  std::uint32_t source);
	// The game's thread: where the listener is and faces; every voice is
	// heard afresh from there
	void SetListener(const vector2d& ear, double theta);
	// The effects' volume, 0 to 1
	void SetVolume(float volume) {
		volume_.store(volume, std::memory_order_relaxed);
	}

	// The audio thread: takes what was asked, then adds the voices into
	// `stream` (16-bit stereo, `bytes` long), moving each on; a voice that
	// reaches its end is free again
	void Mix(std::uint8_t* stream, int bytes);
	// The audio thread (or a test, mixing itself): voices playing
	std::size_t Playing() const;

	// The hook SDL_mixer calls with its mix (Mix_SetPostMix), `mixer` this
	static void MixHook(void* mixer, std::uint8_t* stream, int bytes);

  private:
	struct Command
	{
		enum class Kind : std::uint8_t { Play, Listen };
		Kind kind = Kind::Play;
		const Mix_Chunk* chunk = nullptr;
		vector2d where{};  // the sound's place, or the listener's
		double theta = 0.0;
		bool muffled = false;
		std::uint32_t source = 0;
	};
	struct Voice
	{
		const Mix_Chunk* chunk = nullptr;
		std::uint32_t at = 0;  // in sample frames
		vector2d where{};
		bool muffled = false;
		std::uint32_t source = 0;
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

}  // namespace wolfenstein

#endif	// SOUND_MANAGER_INCLUDE_SOUND_MANAGER_SPATIAL_MIXER_H
