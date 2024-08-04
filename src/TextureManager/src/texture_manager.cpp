#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_image.h>

namespace wolfenstein {

TextureManager* TextureManager::instance_ = nullptr;

TextureManager& TextureManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new TextureManager();
	}
	return *instance_;
}

void TextureManager::InitManager(SDL_Renderer* renderer) {
	renderer_ = renderer;

	std::string texture_path = std::string(RESOURCE_DIR) + "textures/";
	LoadTexture(0, texture_path + "sky.png");
	LoadTexture(1, texture_path + "1.png");
	LoadTexture(2, texture_path + "2.png");
	LoadTexture(3, texture_path + "3.png");
	LoadTexture(4, texture_path + "4.png");
	LoadTexture(5, texture_path + "5.png");
	LoadTexture(6, texture_path + "candlebra.png");
}

void TextureManager::LoadTexture(uint16_t texture_id,
								 const std::string& texture_path) {
	textures_[texture_id] = IMG_LoadTexture(renderer_, texture_path.c_str());
}

SDL_Texture* TextureManager::GetTexture(uint16_t texture_id) {
	return textures_[texture_id];
}

}  // namespace wolfenstein
