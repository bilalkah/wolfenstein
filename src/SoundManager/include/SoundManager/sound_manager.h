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
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace wolfenstein {

class SoundManager
{

  public:
	static SoundManager& GetInstance();

	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;
	~SoundManager();

	void InitManager();
	void PlayEffect(std::string requester_id, std::string sound_effect);

  private:
	SoundManager() = default;
	void LoadSound(const std::string sound_name, const std::string& sound_path);
	void PlaySound(Mix_Chunk* sound, int channel = -1, int loops = 0);
	Mix_Chunk* GetSound(std::string sound_name);

	static SoundManager* instance_;
	bool initialized_{false};
	int channel_counter;
	std::unordered_map<std::string, Mix_Chunk*> chunks_;
	std::unordered_map<std::string, int> id_to_channel_;
	Mix_Music* main_theme;
};

}  // namespace wolfenstein

#endif	// SOUND_MANAGER_INCLUDE_SOUND_MANAGER_H
