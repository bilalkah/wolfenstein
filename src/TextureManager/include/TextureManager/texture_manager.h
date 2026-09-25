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
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wolfenstein {

struct Texture
{
	Texture() = default;
	Texture(SDL_Texture* texture, int width, int height)
		: texture(texture), width(width), height(height) {}
	SDL_Texture* texture{};
	int width{};
	int height{};
};

class TextureManager
{

  public:
	static TextureManager& GetInstance();

	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;
	~TextureManager();

	void InitManager(SDL_Renderer* renderer);

	void LoadTexture(uint16_t texture_id, const std::string& texture_path);
	// Textures are stored by id, which runs densely from 0, so a lookup is
	// an index rather than a hash (it runs for every draw call). References
	// stay valid once InitManager has loaded everything.
	Texture& GetTexture(uint16_t texture_id) {
		assert(texture_id < textures_.size() && "texture not loaded");
		return textures_[texture_id];
	}
	// The frames of a named animation clip ("soldier_walk"). The ids live
	// here for the program's life, so every animation playing the clip shares
	// them instead of holding a copy. Exits for an unknown name: the assets
	// and the code disagree.
	std::span<const std::uint16_t> GetTextureCollection(
		std::string_view collection_name) const;
	// Names the textures [begin, end) as a clip. InitManager defines the
	// game's clips; tests without a renderer define placeholders.
	void DefineCollection(std::string key, uint16_t begin, uint16_t end);

  private:
	TextureManager() = default;
	void LoadStaticTextures();
	void LoadSpriteTextures();
	void LoadNpcTextures();
	void LoadWeaponTextures();

	static TextureManager* instance_;
	uint16_t t_count_{};
	SDL_Renderer* renderer_{};
	std::vector<Texture> textures_;
	// Transparent hashing: looked up with a string_view, no key string built
	struct StringHash
	{
		using is_transparent = void;
		std::size_t operator()(std::string_view key) const noexcept {
			return std::hash<std::string_view>{}(key);
		}
	};
	std::unordered_map<std::string, std::vector<uint16_t>, StringHash,
					   std::equal_to<>>
		texture_collections_;
};

}  // namespace wolfenstein

#endif	// TEXTURE_MANAGER_INCLUDE_TEXTURE_MANAGER_H
