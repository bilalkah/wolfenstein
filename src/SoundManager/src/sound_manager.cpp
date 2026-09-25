#include "SoundManager/sound_manager.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace wolfenstein {

SoundManager* SoundManager::instance_ = nullptr;

SoundManager& SoundManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new SoundManager();
	}
	return *instance_;
}

SoundManager::~SoundManager() {
	for (Mix_Chunk* chunk : chunks_) {
		Mix_FreeChunk(chunk);
	}
	Mix_FreeMusic(main_theme);
	Mix_CloseAudio();
	delete instance_;
}

void SoundManager::SetMasterVolume(double volume) {
	constexpr int kMusicVolume = 64;  // the theme's level at full volume
	const double clamped = std::clamp(volume, 0.0, 1.0);
	Mix_VolumeMusic(static_cast<int>(kMusicVolume * clamped));
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
	Mix_MasterVolume(static_cast<int>(MIX_MAX_VOLUME * clamped));
#else
	Mix_Volume(-1, static_cast<int>(MIX_MAX_VOLUME * clamped));
#endif
}

void SoundManager::InitManager() {
	if (initialized_) {
		return;
	}
	if (SDL_Init(SDL_INIT_AUDIO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		std::cerr << "Failed to initialize SDL_mixer: " << Mix_GetError()
				  << std::endl;
		exit(EXIT_FAILURE);
	}

	// Allocate channels (default is 8; increase if necessary)
	Mix_AllocateChannels(kChannels);

	const std::string sound_path = std::string(RESOURCE_DIR) + "sounds/";
	LoadSound(SoundEffect::NpcAttack, sound_path + "npc_attack.wav", 32);
	LoadSound(SoundEffect::NpcPain, sound_path + "npc_pain.wav", 32);
	LoadSound(SoundEffect::NpcDeath, sound_path + "npc_death.wav", 64);
	LoadSound(SoundEffect::PlayerPain, sound_path + "player_pain.wav", 64);
	LoadSound(SoundEffect::Shotgun, sound_path + "shotgun.wav", 64);

	std::string main_music = sound_path + "theme.mp3";
	main_theme = Mix_LoadMUS(main_music.c_str());
	Mix_VolumeMusic(64);

	// Play the MP3 file
	if (Mix_PlayMusic(main_theme, -1) == -1) {
		std::cerr << "Failed to play MP3 file: " << Mix_GetError() << std::endl;
		exit(EXIT_FAILURE);
	}
	initialized_ = true;
}

SoundChannel SoundManager::AllocateChannel() {
	const int channel = next_channel_;
	next_channel_ = (next_channel_ + 1) % kChannels;
	return SoundChannel{channel};
}

void SoundManager::PlayEffect(SoundChannel channel, SoundEffect effect) {
	const int index = std::to_underlying(channel);
	Mix_HaltChannel(index);
	if (Mix_PlayChannel(index, chunks_[std::to_underlying(effect)], 0) == -1) {
		std::cerr << "Failed to play sound: " << Mix_GetError() << std::endl;
	}
}

void SoundManager::LoadSound(SoundEffect effect, const std::string& sound_path,
							 int volume) {
	Mix_Chunk* sound = Mix_LoadWAV(sound_path.c_str());
	if (!sound) {
		std::cerr << "Failed to load WAV file: " << Mix_GetError() << std::endl;
		std::exit(EXIT_FAILURE);
	}
	Mix_VolumeChunk(sound, volume);
	chunks_[std::to_underlying(effect)] = sound;
}

}  // namespace wolfenstein
