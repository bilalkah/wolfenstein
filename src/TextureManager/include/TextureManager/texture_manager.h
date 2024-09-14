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

#ifndef TEXTURE_MANAGER_INCLUDE_TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_INCLUDE_TEXTURE_MANAGER_H

#include <SDL2/SDL.h>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace wolfenstein {

struct Texture
{
	SDL_Texture* texture;
	int width;
	int height;
};

class TextureManager
{

  public:
	static TextureManager& GetInstance();

	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;
	~TextureManager() = default;

	void InitManager(SDL_Renderer* renderer);

	void LoadTexture(uint16_t texture_id, const std::string& texture_path);
	Texture GetTexture(uint16_t texture_id);
	std::vector<uint16_t> GetTextureCollection(
		const std::string& collection_name);

  private:
	TextureManager() = default;
	void LoadStaticTextures();
	void LoadSpriteTextures();
	void LoadNpcTextures();
	void LoadWeaponTextures();

	static TextureManager* instance_;
	std::unordered_map<uint16_t, Texture> textures_;
	std::unordered_map<std::string, std::vector<uint16_t>> texture_collections_;
	SDL_Renderer* renderer_;
	size_t texture_count_;
};

}  // namespace wolfenstein

#endif	// TEXTURE_MANAGER_INCLUDE_TEXTURE_MANAGER_H
