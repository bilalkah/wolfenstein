#include "SoundManager/sound_manager.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace wolfenstein {

std::expected<std::unique_ptr<SoundManager>, std::string> SoundManager::Open(
	const std::string& sound_dir) {
	const auto error = [](std::string_view what, const char* detail) {
		return std::unexpected(std::string(what) + ": " + detail);
	};
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		return error("cannot initialise SDL audio", SDL_GetError());
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return error("cannot open the audio device", Mix_GetError());
	}
	// From here the destructor closes the device, whatever fails next
	auto sound = std::make_unique<SoundManager>();
	sound->open_ = true;
	Mix_AllocateChannels(kChannels);

	for (const auto& [effect, file, volume] :
		 {std::tuple{SoundEffect::NpcAttack, "npc_attack.wav", 32},
		  std::tuple{SoundEffect::NpcPain, "npc_pain.wav", 32},
		  std::tuple{SoundEffect::NpcDeath, "npc_death.wav", 64},
		  std::tuple{SoundEffect::PlayerPain, "player_pain.wav", 64},
		  std::tuple{SoundEffect::Shotgun, "shotgun.wav", 64},
		  std::tuple{SoundEffect::Pickup, "pickup.wav", 64},
		  std::tuple{SoundEffect::PistolShot, "pistol.wav", 64},
		  std::tuple{SoundEffect::SmgShot, "mp5.wav", 64},
		  std::tuple{SoundEffect::DryFire, "dry_fire.wav", 64},
		  std::tuple{SoundEffect::PlayerFall, "player_fall.wav", 64}}) {
		if (auto loaded = sound->LoadSound(effect, sound_dir + file, volume);
			!loaded) {
			return std::unexpected(loaded.error());
		}
	}

	const std::string theme = sound_dir + "theme.mp3";
	sound->main_theme = Mix_LoadMUS(theme.c_str());
	if (sound->main_theme == nullptr) {
		return error("cannot load " + theme, Mix_GetError());
	}
	Mix_VolumeMusic(64);
	if (Mix_PlayMusic(sound->main_theme, -1) == -1) {
		return error("cannot play " + theme, Mix_GetError());
	}
	return sound;
}

SoundManager::~SoundManager() {
	if (!open_) {
		return;
	}
	Mix_HaltMusic();
	Mix_HaltChannel(-1);
	for (Mix_Chunk* chunk : chunks_) {
		Mix_FreeChunk(chunk);
	}
	Mix_FreeMusic(main_theme);
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// Changes the audio device, not the manager's members, but is not const:
// it changes what the manager plays
// NOLINTNEXTLINE(readability-make-member-function-const)
void SoundManager::SetMasterVolume(double volume) {
	if (!open_) {
		return;
	}
	constexpr int kMusicVolume = 64;  // the theme's level at full volume
	const double clamped = std::clamp(volume, 0.0, 1.0);
	Mix_VolumeMusic(static_cast<int>(kMusicVolume * clamped));
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
	Mix_MasterVolume(static_cast<int>(MIX_MAX_VOLUME * clamped));
#else
	Mix_Volume(-1, static_cast<int>(MIX_MAX_VOLUME * clamped));
#endif
}

SoundChannel SoundManager::AllocateChannel() {
	const int channel = next_channel_;
	next_channel_ = (next_channel_ + 1) % kChannels;
	return SoundChannel{channel};
}

void SoundManager::PlayEffect(SoundChannel channel, SoundEffect effect) {
	if (!open_) {
		return;
	}
	const int index = std::to_underlying(channel);
	Mix_HaltChannel(index);
	if (Mix_PlayChannel(index, chunks_[std::to_underlying(effect)], 0) == -1) {
		std::cerr << "Failed to play sound: " << Mix_GetError() << '\n';
	}
}

std::expected<void, std::string> SoundManager::LoadSound(
	SoundEffect effect, const std::string& sound_path, int volume) {
	Mix_Chunk* sound = Mix_LoadWAV(sound_path.c_str());
	if (sound == nullptr) {
		return std::unexpected("cannot load " + sound_path + ": " +
							   Mix_GetError());
	}
	Mix_VolumeChunk(sound, volume);
	chunks_[std::to_underlying(effect)] = sound;
	return {};
}

}  // namespace wolfenstein
