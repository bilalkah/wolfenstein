#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_image.h>
#include <string>

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
	t_count_ = 0;

	LoadStaticTextures();
	LoadSpriteTextures();
	LoadNpcTextures();
	LoadWeaponTextures();
}

void TextureManager::LoadTexture(uint16_t texture_id,
								 const std::string& texture_path) {
	Texture texture;
	texture.texture = IMG_LoadTexture(renderer_, texture_path.c_str());
	SDL_QueryTexture(texture.texture, nullptr, nullptr, &texture.width,
					 &texture.height);
	textures_[texture_id] = texture;
}

Texture TextureManager::GetTexture(uint16_t texture_id) {
	return textures_[texture_id];
}

std::vector<uint16_t> TextureManager::GetTextureCollection(
	const std::string& collection_name) {
	return texture_collections_[collection_name];
}

void TextureManager::LoadStaticTextures() {
	// Static textures
	std::string texture_path = std::string(RESOURCE_DIR) + "textures/";
	LoadTexture(t_count_++, texture_path + "sky.png");
	LoadTexture(t_count_++, texture_path + "1.png");
	LoadTexture(t_count_++, texture_path + "2.png");
	LoadTexture(t_count_++, texture_path + "3.png");
	LoadTexture(t_count_++, texture_path + "4.png");
	LoadTexture(t_count_++, texture_path + "5.png");
	LoadTexture(t_count_++, texture_path + "crosshair.png");
	LoadTexture(t_count_++, texture_path + "solid_black.png");

	// Digits
	auto begin = t_count_;
	LoadTexture(t_count_++, texture_path + "digits/0.png");
	LoadTexture(t_count_++, texture_path + "digits/1.png");
	LoadTexture(t_count_++, texture_path + "digits/2.png");
	LoadTexture(t_count_++, texture_path + "digits/3.png");
	LoadTexture(t_count_++, texture_path + "digits/4.png");
	LoadTexture(t_count_++, texture_path + "digits/5.png");
	LoadTexture(t_count_++, texture_path + "digits/6.png");
	LoadTexture(t_count_++, texture_path + "digits/7.png");
	LoadTexture(t_count_++, texture_path + "digits/8.png");
	LoadTexture(t_count_++, texture_path + "digits/9.png");
	LoadTexture(t_count_++, texture_path + "digits/10.png");
	FillAscendingIds("digits", begin, t_count_);
}

void TextureManager::LoadSpriteTextures() {
	// Static sprites
	std::string sprite_path = std::string(RESOURCE_DIR) + "sprites/";
	LoadTexture(t_count_++, sprite_path + "static_sprites/candlebra.png");

	// Animated sprites
	auto begin = t_count_;
	LoadTexture(t_count_++, sprite_path + "animated_sprites/green_light/0.png");
	LoadTexture(t_count_++, sprite_path + "animated_sprites/green_light/1.png");
	LoadTexture(t_count_++, sprite_path + "animated_sprites/green_light/2.png");
	LoadTexture(t_count_++, sprite_path + "animated_sprites/green_light/3.png");
	FillAscendingIds("green_light", begin, t_count_);
	begin = t_count_;
	LoadTexture(t_count_++, sprite_path + "animated_sprites/red_light/0.png");
	LoadTexture(t_count_++, sprite_path + "animated_sprites/red_light/1.png");
	LoadTexture(t_count_++, sprite_path + "animated_sprites/red_light/2.png");
	LoadTexture(t_count_++, sprite_path + "animated_sprites/red_light/3.png");
	FillAscendingIds("red_light", begin, t_count_);
}

void TextureManager::LoadNpcTextures() {
	// Npc sprites
	std::string npc_path = std::string(RESOURCE_DIR) + "sprites/npc/";
	// Caco demon Attack
	auto begin = t_count_;
	LoadTexture(t_count_++, npc_path + "caco_demon/attack/0.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/attack/1.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/attack/2.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/attack/3.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/attack/4.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/attack/0.png");
	FillAscendingIds("caco_demon_attack", begin, t_count_);
	// Caco demon Death
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "caco_demon/death/0.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/death/1.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/death/2.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/death/3.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/death/4.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/death/5.png");
	FillAscendingIds("caco_demon_death", begin, t_count_);
	// Caco demon Idle
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "caco_demon/idle/0.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/idle/1.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/idle/7.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/2.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/3.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/1.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/6.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/7.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/5.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/4.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/3.png");
	// LoadTexture(t_count_++, npc_path + "caco_demon/idle/5.png");
	FillAscendingIds("caco_demon_idle", begin, t_count_);
	// Caco demon Pain
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "caco_demon/pain/0.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/pain/1.png");
	FillAscendingIds("caco_demon_pain", begin, t_count_);
	// Caco demon Walk
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "caco_demon/walk/0.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/walk/1.png");
	LoadTexture(t_count_++, npc_path + "caco_demon/walk/2.png");
	FillAscendingIds("caco_demon_walk", begin, t_count_);
	// Cyber demon Attack
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "cyber_demon/attack/0.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/attack/1.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/attack/0.png");
	FillAscendingIds("cyber_demon_attack", begin, t_count_);
	// Cyber demon Death
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/0.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/1.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/2.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/3.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/4.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/5.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/6.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/7.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/death/8.png");
	FillAscendingIds("cyber_demon_death", begin, t_count_);
	// Cyber demon Idle
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "cyber_demon/idle/0.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/idle/1.png");
	// LoadTexture(t_count_++, npc_path + "cyber_demon/idle/2.png");
	// LoadTexture(t_count_++, npc_path + "cyber_demon/idle/3.png");
	// LoadTexture(t_count_++, npc_path + "cyber_demon/idle/4.png");
	// LoadTexture(t_count_++, npc_path + "cyber_demon/idle/5.png");
	// LoadTexture(t_count_++, npc_path + "cyber_demon/idle/6.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/idle/7.png");
	FillAscendingIds("cyber_demon_idle", begin, t_count_);
	// Cyber demon Pain
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "cyber_demon/pain/0.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/pain/1.png");
	FillAscendingIds("cyber_demon_pain", begin, t_count_);
	// Cyber demon Walk
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "cyber_demon/walk/0.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/walk/1.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/walk/3.png");
	LoadTexture(t_count_++, npc_path + "cyber_demon/walk/4.png");
	FillAscendingIds("cyber_demon_walk", begin, t_count_);

	// Soldier Attack
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "soldier/attack/0.png");
	LoadTexture(t_count_++, npc_path + "soldier/attack/1.png");
	LoadTexture(t_count_++, npc_path + "soldier/attack/0.png");
	FillAscendingIds("soldier_attack", begin, t_count_);
	// Soldier Death
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "soldier/death/0.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/1.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/2.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/3.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/4.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/5.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/6.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/7.png");
	LoadTexture(t_count_++, npc_path + "soldier/death/8.png");
	FillAscendingIds("soldier_death", begin, t_count_);
	// Soldier Idle
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "soldier/idle/0.png");
	LoadTexture(t_count_++, npc_path + "soldier/idle/1.png");
	// LoadTexture(t_count_++, npc_path + "soldier/idle/2.png");
	// LoadTexture(t_count_++, npc_path + "soldier/idle/3.png");
	// LoadTexture(t_count_++, npc_path + "soldier/idle/4.png");
	// LoadTexture(t_count_++, npc_path + "soldier/idle/5.png");
	// LoadTexture(t_count_++, npc_path + "soldier/idle/6.png");
	LoadTexture(t_count_++, npc_path + "soldier/idle/7.png");
	FillAscendingIds("soldier_idle", begin, t_count_);
	// Soldier Pain
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "soldier/pain/0.png");
	FillAscendingIds("soldier_pain", begin, t_count_);
	// Soldier Walk
	begin = t_count_;
	LoadTexture(t_count_++, npc_path + "soldier/walk/0.png");
	LoadTexture(t_count_++, npc_path + "soldier/walk/1.png");
	LoadTexture(t_count_++, npc_path + "soldier/walk/2.png");
	LoadTexture(t_count_++, npc_path + "soldier/walk/3.png");
	FillAscendingIds("soldier_walk", begin, t_count_);
}

void TextureManager::LoadWeaponTextures() {
	const auto weapon_path = std::string(RESOURCE_DIR) + "sprites/weapon/";
	// MP5 sprites
	// Loaded
	auto begin = t_count_;
	LoadTexture(t_count_++, weapon_path + "/mp5/loaded/" + "0.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/loaded/" + "1.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/loaded/" + "2.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/loaded/" + "3.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/loaded/" + "4.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/loaded/" + "5.png");
	FillAscendingIds("mp5_loaded", begin, t_count_);
	// outofammo
	begin = t_count_;
	LoadTexture(t_count_++, weapon_path + "/mp5/outofammo/" + "0.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/outofammo/" + "1.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/outofammo/" + "2.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/outofammo/" + "3.png");
	FillAscendingIds("mp5_outofammo", begin, t_count_);
	// reload
	begin = t_count_;
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "0.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "1.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "2.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "3.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "4.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "5.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "6.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "7.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "8.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "9.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "10.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "11.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "12.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "13.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "14.png");
	LoadTexture(t_count_++, weapon_path + "/mp5/reload/" + "15.png");
	FillAscendingIds("mp5_reload", begin, t_count_);

	// Shotgun sprites
	// Loaded
	begin = t_count_;
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "0.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "1.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "2.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "3.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "4.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "5.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "6.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "7.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "8.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/loaded/" + "9.png");
	FillAscendingIds("shotgun_loaded", begin, t_count_);
	// outofammo
	begin = t_count_;
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "0.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "1.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "2.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "3.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "4.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "5.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "6.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/outofammo/" + "7.png");
	FillAscendingIds("shotgun_outofammo", begin, t_count_);
	// reload
	begin = t_count_;
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "0.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "1.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "2.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "3.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "4.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "5.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "6.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "7.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "8.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "9.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "10.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "11.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "12.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "13.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "14.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "15.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "16.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "17.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "18.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "19.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "20.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "21.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "22.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "23.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "24.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "25.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "26.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "27.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "28.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "29.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "30.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "31.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "32.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "33.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "34.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "35.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "36.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "37.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "38.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "39.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "40.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "41.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "42.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "43.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "44.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "45.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "46.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "47.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "48.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "49.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "50.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "51.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "52.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "53.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "54.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "55.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "56.png");
	LoadTexture(t_count_++, weapon_path + "/shotgun/reload/" + "57.png");
	FillAscendingIds("shotgun_reload", begin, t_count_);
}

void TextureManager::FillAscendingIds(std::string key, uint16_t begin,
									  uint16_t end) {
	std::vector<uint16_t> textures;
	for (uint16_t i = begin; i < end; i++) {
		textures.push_back(i);
	}
	texture_collections_[key] = textures;
}

}  // namespace wolfenstein
