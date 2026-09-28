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

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
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

// What the mixer plays at, 0 to MIX_MAX_VOLUME: the music, and every effect
// (each keeps its own level under that). `master`, `music` and `effects`
// run 0 (silent) to 1 (full); music and effects are shares of the master.
struct MixerLevels
{
	int music = 0;
	int effects = 0;
};
MixerLevels ToMixerLevels(double master, double music, double effects);

// The mixer channel a sound source plays on: a new sound from the source
// cuts off its previous one, never another source's
enum class SoundChannel : int {};

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
	// for its life. Sources share the mixer channels round-robin.
	SoundChannel AllocateChannel();
	// Effects are an enum indexing an array, so playing one involves no
	// string or lookup (a string name allocated on wasm32, whose short-string
	// buffer holds only 10 characters)
	void PlayEffect(SoundChannel channel, SoundEffect effect);
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
	std::expected<void, std::string> LoadSound(SoundEffect effect,
											   const std::string& sound_path,
											   int volume);

	bool open_{false};
	// Mixer channels allocated when the device is opened
	static constexpr int kChannels = 16;
	int next_channel_{};
	std::array<Mix_Chunk*, kSoundEffectCount> chunks_{};
	std::array<std::uint32_t, kSoundEffectCount> play_counts_{};
	struct Track
	{
		std::string name;
		Mix_Music* music;
	};
	std::vector<Track> tracks_;
	std::string_view playing_;
};

}  // namespace wolfenstein

#endif	// SOUND_MANAGER_INCLUDE_SOUND_MANAGER_H
