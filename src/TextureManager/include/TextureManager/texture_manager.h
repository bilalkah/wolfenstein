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
#include <expected>
#include <functional>
#include <istream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
	// The average colour of the image's top rows: a sky's, carried on above
	// it when the player looks higher than the image reaches
	SDL_Color top_colour{0, 0, 0, 255};
};

// assets/textures.json: which images the game loads, as paths under the
// asset directory. New art is added there, not in code.
struct TextureManifest
{
	// Textures the code asks for by name ("sky", "crosshair")
	std::vector<std::pair<std::string, std::string>> textures;
	// The wall textures, by map cell: cell 1 uses walls[0]
	std::vector<std::string> walls;
	// Animation clips ("soldier_walk"), as their frames in order; a frame
	// may repeat, and an image used by several clips is loaded once
	std::vector<std::pair<std::string, std::vector<std::string>>> clips;
};

std::expected<TextureManifest, std::string> ParseTextureManifest(
	std::istream& input);

// The game's textures and the animation clips over them. Textures belong to
// the SDL renderer that created them, so the renderer's owner (the
// RendererContext) owns this too and destroys it before the renderer.
class TextureManager
{

  public:
	// No textures: for tests, which define placeholder clips instead
	TextureManager() = default;
	// Loads every image the manifest names; the error says which one failed
	static std::expected<std::unique_ptr<TextureManager>, std::string> Load(
		SDL_Renderer* renderer, const TextureManifest& manifest,
		const std::string& asset_dir);
	~TextureManager();
	// Owns SDL textures, and animations keep spans into its clips
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;
	TextureManager(TextureManager&&) = delete;
	TextureManager& operator=(TextureManager&&) = delete;

	// Textures are stored by id, which runs densely from 0, so a lookup is
	// an index rather than a hash (it runs for every draw call). References
	// stay valid for the manager's life. Ids come from maps, animations and
	// rays as int.
	Texture& GetTexture(int texture_id) {
		assert(texture_id >= 0 &&
			   static_cast<std::size_t>(texture_id) < textures_.size() &&
			   "texture not loaded");
		return textures_[static_cast<std::size_t>(texture_id)];
	}
	// The id of a texture the manifest names; looked up once, when a
	// renderer is built, not per frame. Exits for an unknown name: the
	// manifest and the code disagree.
	int GetTextureId(std::string_view name) const;
	int TextureCount() const { return static_cast<int>(textures_.size()); }
	// The texture of a map wall cell (1 and up); exits for a cell the
	// manifest has no wall for
	int GetWallTexture(int cell) const;
	// The frames of a named animation clip ("soldier_walk"). The ids live
	// here for the manager's life, so every animation playing the clip
	// shares them instead of holding a copy. Exits for an unknown name.
	std::span<const std::uint16_t> GetTextureCollection(
		std::string_view collection_name) const;
	// The frames of a clip art may leave out ("mp5_raise"); empty if absent
	std::span<const std::uint16_t> FindTextureCollection(
		std::string_view collection_name) const;
	// Names the textures [begin, end) as a clip: tests without a renderer
	// define placeholders
	void DefineCollection(std::string key, uint16_t begin, uint16_t end);
	// Names the texture `id` (a placeholder, likewise)
	void DefineTexture(std::string name, int id);

  private:
	// Transparent hashing: looked up with a string_view, no key string built
	struct StringHash
	{
		using is_transparent = void;
		std::size_t operator()(std::string_view key) const noexcept {
			return std::hash<std::string_view>{}(key);
		}
	};
	template <typename V>
	using StringMap =
		std::unordered_map<std::string, V, StringHash, std::equal_to<>>;

	std::vector<Texture> textures_;
	StringMap<int> named_;
	std::vector<int> walls_;
	StringMap<std::vector<uint16_t>> texture_collections_;
};

}  // namespace wolfenstein

#endif	// TEXTURE_MANAGER_INCLUDE_TEXTURE_MANAGER_H
