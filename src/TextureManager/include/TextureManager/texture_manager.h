/**
 * @file texture_manager.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef TEXTURE_MANAGER_TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_TEXTURE_MANAGER_H

#include <string>
#include <unordered_map>

#include <SDL2/SDL.h>

namespace wolfenstein {

class TextureManager
{

  public:
	static TextureManager& GetInstance();

	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;
	~TextureManager() = default;

	void InitManager(SDL_Renderer* renderer);

	void LoadTexture(uint16_t texture_id, const std::string& texture_path);
	SDL_Texture* GetTexture(uint16_t texture_id);

  private:
	TextureManager() = default;

	static TextureManager* instance_;
	std::unordered_map<uint16_t, SDL_Texture*> textures_;
	SDL_Renderer* renderer_;
};

}  // namespace wolfenstein

#endif	// TEXTURE_MANAGER_TEXTURE_MANAGER_H
