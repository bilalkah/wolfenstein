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
#include <string>
#include <utility>

namespace wolfenstein {

enum class SoundEffect : std::uint8_t {
	NpcAttack,
	NpcPain,
	NpcDeath,
	PlayerPain,
	Shotgun,
};
inline constexpr std::size_t kSoundEffectCount = 5;
static_assert(std::to_underlying(SoundEffect::Shotgun) + 1 ==
			  kSoundEffectCount);

// The mixer channel a sound source plays on: a new sound from the source
// cuts off its previous one, never another source's
enum class SoundChannel : int {};

class SoundManager
{

  public:
	static SoundManager& GetInstance();

	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;
	~SoundManager();

	void InitManager();
	// 0 (silent) to 1 (full); scales music and effects together
	void SetMasterVolume(double volume);
	// A channel for a new sound source (an enemy, the player, a weapon), kept
	// for its life. Sources share the mixer channels round-robin.
	SoundChannel AllocateChannel();
	// Effects are an enum indexing an array, so playing one involves no
	// string or lookup (a string name allocated on wasm32, whose short-string
	// buffer holds only 10 characters)
	void PlayEffect(SoundChannel channel, SoundEffect effect);

  private:
	SoundManager() = default;
	void LoadSound(SoundEffect effect, const std::string& sound_path,
				   int volume);

	static SoundManager* instance_;
	bool initialized_{false};
	// Mixer channels allocated in InitManager
	static constexpr int kChannels = 16;
	int next_channel_{};
	std::array<Mix_Chunk*, kSoundEffectCount> chunks_{};
	Mix_Music* main_theme{};
};

}  // namespace wolfenstein

#endif	// SOUND_MANAGER_INCLUDE_SOUND_MANAGER_H
