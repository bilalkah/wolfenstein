/**
 * @file sound_manager.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-12
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SOUND_MANAGER_INCLUDE_SOUND_MANAGER_H
#define SOUND_MANAGER_INCLUDE_SOUND_MANAGER_H

#include "Math/vector.h"
#include "SoundManager/spatial_mixer.h"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wolfenstein {

enum class SoundEffect : std::uint8_t {
	NpcAttack,
	NpcPain,
	NpcDeath,
	PlayerPain,
	Shotgun,
	Pickup,
	PistolShot,
	SmgShot,
	DryFire,	 // the trigger pulled on an empty gun
	PlayerFall,	 // the player's body landing, dead
	DoorMove,	 // a door sliding open or shut
	EnemyAlert,	 // an enemy that has seen or heard the player
	StepLeft,	 // the player's footsteps, one foot and the other
	StepRight,
	AmmoPickup,
	KeyPickup,
	WeaponPickup,
	// Enemies' own voices, by kind (config.json names them)
	DemonAttack,  // a bite
	DemonAlert,
	DemonPain,
	DemonDeath,
	CacoAlert,
	CacoDeath,
	CyberAlert,
	CyberDeath,
	ZombieAlert,  // the tougher zombies'
	ZombieDeath,
	// Weapons found later
	SuperShotgun,
	SuperShotgunReload,	 // broken open, loaded and snapped shut
	SawUp,				 // the saw starting as it is taken in hand
	Saw,				 // cutting air
	SawHit,				 // cutting an enemy
	RocketLaunch,
	RocketBurst,
	Plasma,
	PlasmaBurst,
};
inline constexpr std::size_t kSoundEffectCount = 36;
static_assert(std::to_underlying(SoundEffect::PlasmaBurst) + 1 ==
			  kSoundEffectCount);

// What the mixer plays at, as gains (1 as recorded): the music, and every
// effect (each keeps its own level under that). `master`, `music` and
// `effects` run 0 (silent) to 1 (full); music and effects are shares of the
// master.
struct MixerLevels
{
	float music = 0.0F;
	float effects = 0.0F;
};
MixerLevels ToMixerLevels(double master, double music, double effects);

// The channel a sound source plays on: a new sound from the source cuts off
// its previous one, never another source's
enum class SoundChannel : std::uint32_t {};

// The audio device, the sound effects and the music. Owned by the World,
// which destroys it (closing the device) before SDL shuts down.
class SoundManager
{

  public:
	// Silent: no audio device, so effects and volume changes do nothing
	// (tests and headless tools)
	SoundManager() = default;
	// Opens the audio device and loads every sound from sound_dir and every
	// track named (music_dir/<name>.mp3), playing none yet; the error says
	// what failed
	static std::expected<std::unique_ptr<SoundManager>, std::string> Open(
		const std::string& sound_dir, const std::string& music_dir,
		std::span<const std::string> tracks);
	~SoundManager();
	// Owns the device and the loaded sounds
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;
	SoundManager(SoundManager&&) = delete;
	SoundManager& operator=(SoundManager&&) = delete;

	// 0 (silent) to 1 (full) each; music and effects are shares of the
	// master volume
	void SetVolume(double master, double music, double effects);
	// A channel for a new sound source (an enemy, the player, a weapon), kept
	// for its life. Sources share the channels round-robin.
	SoundChannel AllocateChannel();
	// Effects are an enum indexing an array, so playing one involves no
	// string or lookup (a string name allocated on wasm32, whose short-string
	// buffer holds only 10 characters). The player's own sounds (its gun,
	// its steps) play here, as heard from nowhere in particular.
	void PlayEffect(SoundChannel channel, SoundEffect effect);
	// A sound from a place in the level (an enemy's, a door's): heard from
	// that side of the listener, quieter the further off, and muffled
	// through a wall. A `source` other than 0 is one voice: its new sound
	// cuts off its last. Allocates nothing.
	void PlayAt(SoundEffect effect, const vector2d& where, bool muffled,
				std::uint32_t source = 0);
	// Where the one hearing is, and which way they face: every sound from a
	// place is heard afresh from there
	void SetListener(const vector2d& ear, double theta);
	// Plays the named track, over and over, fading in; the one playing
	// already goes on, and an unknown name is silence. The name must outlive
	// the manager (a level's, from its data).
	void PlayMusic(std::string_view name);
	// The track last asked for, playing or not (tests listen through this)
	std::string_view Playing() const { return playing_; }
	// How many times an effect was asked for, heard or not (tests listen
	// through this)
	std::uint32_t PlayCount(SoundEffect effect) const {
		return play_counts_[std::to_underlying(effect)];
	}

  private:
	// Reads a WAV file as the effects are mixed: float stereo at the
	// mixer's rate. `volume` is its level, 0 to 128.
	std::expected<void, std::string> LoadSound(SoundEffect effect,
											   const std::string& sound_path,
											   int volume);
	// The next `frames` sample frames of what is heard: the music, then the
	// effects over it
	void Generate(int frames);
	// SDL's audio thread asks the device's stream for more to play
	static void SDLCALL Feed(void* manager, SDL_AudioStream* stream,
							 int additional_amount, int total_amount);

	bool open_{false};
	// The music's mixer, which the game drives (Generate) rather than the
	// device: every track can be played a moment at startup (see Open)
	MIX_Mixer* mixer_ = nullptr;
	SDL_AudioStream* device_ = nullptr;
	// Mixes the effects, the player's own and those from places
	SpatialMixer spatial_;
	// What Generate fills, a piece of the device's request at a time
	static constexpr int kFeedFrames = 2048;
	std::array<float, std::size_t{2} * kFeedFrames> feed_{};
	// Channels the sources share
	static constexpr std::uint32_t kChannels = 16;
	std::uint32_t next_channel_{};
	std::array<std::vector<float>, kSoundEffectCount> samples_{};
	std::array<SoundClip, kSoundEffectCount> clips_{};
	std::array<std::uint32_t, kSoundEffectCount> play_counts_{};
	// Every track has its own SDL_mixer track, given its music once, at
	// startup: giving a track other music allocates
	struct Track
	{
		std::string name;
		MIX_Audio* audio = nullptr;
		MIX_Track* track = nullptr;
	};
	std::vector<Track> tracks_;
	Track* current_ = nullptr;	// the one playing, if any
	// How music plays: over and over, fading in
	SDL_PropertiesID music_options_ = 0;
	std::string_view playing_;
};

}  // namespace wolfenstein

#endif	// SOUND_MANAGER_INCLUDE_SOUND_MANAGER_H
