#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_image.h>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numeric>

namespace wolfenstein {

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
		texture.texture = IMG_LoadTexture(renderer, full_path.c_str());
		if (texture.texture == nullptr) {
			return std::unexpected("cannot load " + full_path + ": " +
								   IMG_GetError());
		}
		SDL_QueryTexture(texture.texture, nullptr, nullptr, &texture.width,
						 &texture.height);
		const auto id = static_cast<int>(manager->textures_.size());
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
