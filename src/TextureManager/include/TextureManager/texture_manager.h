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

// The game's textures and the animation clips over them. Textures belong to
// the SDL renderer that created them, so the renderer's owner (the
// RendererContext) owns this too and destroys it before the renderer.
class TextureManager
{

  public:
	// No textures: for tests, which define placeholder clips instead
	TextureManager() = default;
	// Loads every texture and clip for the renderer
	explicit TextureManager(SDL_Renderer* renderer);
	~TextureManager();
	// Owns SDL textures, and animations keep spans into its clips
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;
	TextureManager(TextureManager&&) = delete;
	TextureManager& operator=(TextureManager&&) = delete;

	void LoadTexture(uint16_t texture_id, const std::string& texture_path);
	// Textures are stored by id, which runs densely from 0, so a lookup is
	// an index rather than a hash (it runs for every draw call). References
	// stay valid once the constructor has loaded everything.
	// Ids come from maps, animations and rays as int
	Texture& GetTexture(int texture_id) {
		assert(texture_id >= 0 &&
			   static_cast<std::size_t>(texture_id) < textures_.size() &&
			   "texture not loaded");
		return textures_[static_cast<std::size_t>(texture_id)];
	}
	// The frames of a named animation clip ("soldier_walk"). The ids live
	// here for the program's life, so every animation playing the clip shares
	// them instead of holding a copy. Exits for an unknown name: the assets
	// and the code disagree.
	std::span<const std::uint16_t> GetTextureCollection(
		std::string_view collection_name) const;
	// Names the textures [begin, end) as a clip. The constructor defines the
	// game's clips; tests without a renderer define placeholders.
	void DefineCollection(std::string key, uint16_t begin, uint16_t end);

  private:
	void LoadStaticTextures();
	void LoadSpriteTextures();
	void LoadNpcTextures();
	void LoadWeaponTextures();

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
