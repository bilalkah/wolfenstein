#include "SoundManager/sound_manager.h"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <string>

namespace wolfenstein {

SoundManager* SoundManager::instance_ = nullptr;

SoundManager& SoundManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new SoundManager();
	}
	return *instance_;
}

SoundManager::~SoundManager() {
	for (auto chunk : chunks_) {
		Mix_FreeChunk(chunk.second);
	}
	Mix_FreeMusic(main_theme);
	Mix_CloseAudio();
	delete instance_;
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
	Mix_AllocateChannels(16);

	std::string sound_path = std::string(RESOURCE_DIR) + "sounds/";
	LoadSound("npc_attack", sound_path + "npc_attack.wav");
	LoadSound("npc_pain", sound_path + "npc_pain.wav");
	LoadSound("npc_death", sound_path + "npc_death.wav");
	LoadSound("player_pain", sound_path + "player_pain.wav");
	LoadSound("shotgun", sound_path + "shotgun.wav");

	Mix_VolumeChunk(GetSound("npc_attack"), 32);
	Mix_VolumeChunk(GetSound("npc_pain"), 32);
	Mix_VolumeChunk(GetSound("npc_death"), 64);
	Mix_VolumeChunk(GetSound("player_pain"), 64);
	Mix_VolumeChunk(GetSound("shotgun"), 64);

	std::string main_music = sound_path + "theme.mp3";
	main_theme = Mix_LoadMUS(main_music.c_str());
	Mix_VolumeMusic(64);

	// Play the MP3 file
	if (Mix_PlayMusic(main_theme, -1) == -1) {
		std::cerr << "Failed to play MP3 file: " << Mix_GetError() << std::endl;
		exit(EXIT_FAILURE);
	}
	channel_counter = 0;
	initialized_ = true;
}

// keep track of the ids
void SoundManager::PlayEffect(std::string requester_id,
							  std::string sound_effect) {
	auto channel = id_to_channel_.find(requester_id);
	if (channel == id_to_channel_.end()) {
		id_to_channel_.insert({requester_id, channel_counter++});
		PlaySound(GetSound(sound_effect), id_to_channel_[requester_id]);
	}
	else {
		Mix_HaltChannel(channel->second);
		PlaySound(GetSound(sound_effect), id_to_channel_[requester_id]);
	}
}

void SoundManager::LoadSound(const std::string sound_name,
							 const std::string& sound_path) {
	Mix_Chunk* sound = Mix_LoadWAV(sound_path.c_str());
	if (!sound) {
		std::cerr << "Failed to load WAV file: " << Mix_GetError() << std::endl;
		return exit(EXIT_FAILURE);
	}
	chunks_.insert({sound_name, sound});
}

Mix_Chunk* SoundManager::GetSound(std::string sound_name) {
	return chunks_[sound_name];
}

void SoundManager::PlaySound(Mix_Chunk* sound, int channel, int loops) {
	// Play the sound on the specified channel with the specified number of loops
	if (Mix_PlayChannel(channel, sound, loops) == -1) {
		std::cerr << "Failed to play sound: " << Mix_GetError() << std::endl;
	}
}

}  // namespace wolfenstein
