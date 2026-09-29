#include "TextureManager/texture_manager.h"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numeric>

namespace wolfenstein {

namespace {

// The average colour of an image's top few rows
SDL_Color TopColour(SDL_Surface* image) {
	constexpr int kRows = 4;
	const int rows = std::min(kRows, image->h);
	SDL_Surface* top =
		SDL_CreateSurface(image->w, rows, SDL_PIXELFORMAT_RGBA32);
	if (top == nullptr || rows == 0) {
		SDL_DestroySurface(top);
		return {0, 0, 0, 255};
	}
	// Copied as it is, not blended onto the empty surface
	SDL_BlendMode mode = SDL_BLENDMODE_NONE;
	SDL_GetSurfaceBlendMode(image, &mode);
	SDL_SetSurfaceBlendMode(image, SDL_BLENDMODE_NONE);
	SDL_Rect source{0, 0, image->w, rows};
	SDL_BlitSurface(image, &source, top, nullptr);
	SDL_SetSurfaceBlendMode(image, mode);

	std::array<std::uint64_t, 4> sum{};
	const auto* pixels = static_cast<const std::uint8_t*>(top->pixels);
	for (int y = 0; y < rows; ++y) {
		const auto* row = pixels + static_cast<std::ptrdiff_t>(y) * top->pitch;
		for (int x = 0; x < image->w; ++x) {
			for (std::size_t channel = 0; channel < sum.size(); ++channel) {
				sum[channel] += row[static_cast<std::ptrdiff_t>(x) * 4 +
									static_cast<std::ptrdiff_t>(channel)];
			}
		}
	}
	SDL_DestroySurface(top);
	const auto count =
		static_cast<std::uint64_t>(rows) * static_cast<std::uint64_t>(image->w);
	const auto average = [&](std::size_t channel) {
		return static_cast<std::uint8_t>(sum[channel] / count);
	};
	return {average(0), average(1), average(2), average(3)};
}

// The largest image given a mask: a sprite's; walls and backgrounds are
// bigger and are never shot through
constexpr int kMaskPixels = 512 * 512;

}  // namespace

std::expected<TextureManifest, std::string> ParseTextureManifest(
	std::istream& input) {
	using nlohmann::json;
	try {
		const json root = json::parse(input);
		TextureManifest manifest;
		for (const auto& [name, path] : root.at("textures").items()) {
			manifest.textures.emplace_back(name, path.get<std::string>());
		}
		manifest.walls = root.at("walls").get<std::vector<std::string>>();
		for (const auto& [name, frames] : root.at("clips").items()) {
			auto paths = frames.get<std::vector<std::string>>();
			if (paths.empty()) {
				return std::unexpected("clip " + name + " has no frames");
			}
			manifest.clips.emplace_back(name, std::move(paths));
		}
		return manifest;
	}
	catch (const json::exception& error) {
		return std::unexpected(error.what());
	}
}

std::expected<std::unique_ptr<TextureManager>, std::string>
TextureManager::Load(SDL_Renderer* renderer, const TextureManifest& manifest,
					 const std::string& asset_dir) {
	auto manager = std::make_unique<TextureManager>();
	// Each image is loaded once, however many clips use it
	std::unordered_map<std::string, int> loaded;
	const auto load =
		[&](const std::string& path) -> std::expected<int, std::string> {
		if (const auto found = loaded.find(path); found != loaded.end()) {
			return found->second;
		}
		const std::string full_path = asset_dir + path;
		Texture texture;
		SDL_Surface* image = IMG_Load(full_path.c_str());
		if (image == nullptr) {
			return std::unexpected("cannot load " + full_path + ": " +
								   SDL_GetError());
		}
		texture.top_colour = TopColour(image);
		const auto id = static_cast<int>(manager->textures_.size());
		if (image->w * image->h <= kMaskPixels) {
			SDL_Surface* rgba =
				SDL_ConvertSurface(image, SDL_PIXELFORMAT_RGBA32);
			if (rgba != nullptr) {
				constexpr std::uint8_t kHalf = 128;
				const auto* pixels =
					static_cast<const std::uint8_t*>(rgba->pixels);
				manager->SetMask(
					id, MaskOf(rgba->w, rgba->h, [&](int x, int y) {
						return pixels[static_cast<std::ptrdiff_t>(y) *
										  rgba->pitch +
									  static_cast<std::ptrdiff_t>(x) * 4 + 3] >=
							   kHalf;
					}));
				SDL_DestroySurface(rgba);
			}
		}
		texture.width = image->w;
		texture.height = image->h;
		// Headless (a server) the picture's size and mask are enough
		if (renderer != nullptr) {
			texture.texture = SDL_CreateTextureFromSurface(renderer, image);
			if (texture.texture == nullptr) {
				SDL_DestroySurface(image);
				return std::unexpected("cannot load " + full_path + ": " +
									   SDL_GetError());
			}
		}
		SDL_DestroySurface(image);
		manager->textures_.push_back(texture);
		loaded.emplace(path, id);
		return id;
	};

	for (const auto& [name, path] : manifest.textures) {
		auto id = load(path);
		if (!id) {
			return std::unexpected(id.error());
		}
		manager->named_.emplace(name, *id);
	}
	for (const auto& path : manifest.walls) {
		auto id = load(path);
		if (!id) {
			return std::unexpected(id.error());
		}
		manager->walls_.push_back(*id);
	}
	for (const auto& [name, frames] : manifest.clips) {
		auto& ids = manager->texture_collections_[name];
		for (const auto& path : frames) {
			auto id = load(path);
			if (!id) {
				return std::unexpected(id.error());
			}
			ids.push_back(static_cast<uint16_t>(*id));
		}
	}
	return manager;
}

TextureManager::~TextureManager() {
	for (auto& texture : textures_) {
		if (texture.texture != nullptr) {
			SDL_DestroyTexture(texture.texture);
		}
	}
}

int TextureManager::GetTextureId(std::string_view name) const {
	const auto found = named_.find(name);
	if (found == named_.end()) {
		std::cerr << "Unknown texture: " << name << '\n';
		std::exit(EXIT_FAILURE);
	}
	return found->second;
}

int TextureManager::GetWallTexture(int cell) const {
	if (cell < 1 || static_cast<std::size_t>(cell) > walls_.size()) {
		std::cerr << "No wall texture for map cell " << cell << '\n';
		std::exit(EXIT_FAILURE);
	}
	return walls_[static_cast<std::size_t>(cell - 1)];
}

std::span<const std::uint16_t> TextureManager::GetTextureCollection(
	std::string_view collection_name) const {
	const auto found = texture_collections_.find(collection_name);
	if (found == texture_collections_.end() || found->second.empty()) {
		// An empty clip would divide by zero in every animation using it
		std::cerr << "Unknown texture collection: " << collection_name << '\n';
		std::exit(EXIT_FAILURE);
	}
	return found->second;
}

std::span<const std::uint16_t> TextureManager::FindTextureCollection(
	std::string_view collection_name) const {
	const auto found = texture_collections_.find(collection_name);
	if (found == texture_collections_.end()) {
		return {};
	}
	return found->second;
}

template <typename Solid>
TextureManager::Mask TextureManager::MaskOf(int width, int height,
											Solid solid) {
	Mask mask{.width = width,
			  .height = height,
			  .top = height,
			  .bottom = -1,
			  .bits = {}};
	const auto pixels =
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	mask.bits.resize((pixels + 63) / 64);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (!solid(x, y)) {
				continue;
			}
			const auto index =
				static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
				static_cast<std::size_t>(x);
			mask.bits[index / 64] |= std::uint64_t{1} << (index % 64);
			mask.top = std::min(mask.top, y);
			mask.bottom = std::max(mask.bottom, y);
		}
	}
	return mask;
}

void TextureManager::SetMask(int id, Mask mask) {
	const auto index = static_cast<std::size_t>(id);
	if (masks_.size() <= index) {
		masks_.resize(index + 1);
	}
	masks_[index] = std::move(mask);
}

bool TextureManager::IsSolidAt(int texture_id, double across,
							   double down) const {
	const auto index = static_cast<std::size_t>(texture_id);
	if (index >= masks_.size() || masks_[index].bits.empty()) {
		return true;
	}
	const Mask& mask = masks_[index];
	const int x =
		std::clamp(static_cast<int>(across * mask.width), 0, mask.width - 1);
	const int y =
		std::clamp(static_cast<int>(down * mask.height), 0, mask.height - 1);
	return mask.At(x, y);
}

std::pair<double, double> TextureManager::SolidRows(int texture_id) const {
	const auto index = static_cast<std::size_t>(texture_id);
	if (index >= masks_.size() || masks_[index].bits.empty() ||
		masks_[index].bottom < masks_[index].top) {
		return {0.0, 1.0};
	}
	const Mask& mask = masks_[index];
	return {static_cast<double>(mask.top) / mask.height,
			static_cast<double>(mask.bottom + 1) / mask.height};
}

void TextureManager::DefineMask(int id,
								std::span<const std::string_view> rows) {
	const int width = rows.empty() ? 0 : static_cast<int>(rows[0].size());
	SetMask(id, MaskOf(width, static_cast<int>(rows.size()), [&](int x, int y) {
				return rows[static_cast<std::size_t>(y)]
						   [static_cast<std::size_t>(x)] == '#';
			}));
}

void TextureManager::DefineTexture(std::string name, int id) {
	named_.insert_or_assign(std::move(name), id);
}

void TextureManager::DefineCollection(std::string key, uint16_t begin,
									  uint16_t end) {
	auto& ids = texture_collections_[std::move(key)];
	ids.resize(end - begin);
	std::ranges::iota(ids, begin);
}

}  // namespace wolfenstein
