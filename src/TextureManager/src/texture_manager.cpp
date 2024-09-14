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
	texture_count_ = 0;

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
	LoadTexture(0, texture_path + "sky.png");
	LoadTexture(1, texture_path + "1.png");
	LoadTexture(2, texture_path + "2.png");
	LoadTexture(3, texture_path + "3.png");
	LoadTexture(4, texture_path + "4.png");
	LoadTexture(5, texture_path + "5.png");
	LoadTexture(6, texture_path + "crosshair.png");
	LoadTexture(7, texture_path + "solid_black.png");
}

void TextureManager::LoadSpriteTextures() {
	// Static sprites
	std::string sprite_path = std::string(RESOURCE_DIR) + "sprites/";
	LoadTexture(8, sprite_path + "static_sprites/candlebra.png");

	// Animated sprites
	LoadTexture(9, sprite_path + "animated_sprites/green_light/0.png");
	LoadTexture(10, sprite_path + "animated_sprites/green_light/1.png");
	LoadTexture(11, sprite_path + "animated_sprites/green_light/2.png");
	LoadTexture(12, sprite_path + "animated_sprites/green_light/3.png");
	texture_collections_["green_light"] = {9, 10, 11, 12};
	LoadTexture(13, sprite_path + "animated_sprites/red_light/0.png");
	LoadTexture(14, sprite_path + "animated_sprites/red_light/1.png");
	LoadTexture(15, sprite_path + "animated_sprites/red_light/2.png");
	LoadTexture(16, sprite_path + "animated_sprites/red_light/3.png");
	texture_collections_["red_light"] = {13, 14, 15, 16};
}

void TextureManager::LoadNpcTextures() {
	// Npc sprites
	std::string npc_path = std::string(RESOURCE_DIR) + "sprites/npc/";
	// Caco demon Attack
	LoadTexture(17, npc_path + "caco_demon/attack/0.png");
	LoadTexture(18, npc_path + "caco_demon/attack/1.png");
	LoadTexture(19, npc_path + "caco_demon/attack/2.png");
	LoadTexture(20, npc_path + "caco_demon/attack/3.png");
	LoadTexture(21, npc_path + "caco_demon/attack/4.png");
	texture_collections_["caco_demon_attack"] = {17, 18, 19, 20, 21};
	// Caco demon Death
	LoadTexture(22, npc_path + "caco_demon/death/0.png");
	LoadTexture(23, npc_path + "caco_demon/death/1.png");
	LoadTexture(24, npc_path + "caco_demon/death/2.png");
	LoadTexture(25, npc_path + "caco_demon/death/3.png");
	LoadTexture(26, npc_path + "caco_demon/death/4.png");
	LoadTexture(27, npc_path + "caco_demon/death/5.png");
	texture_collections_["caco_demon_death"] = {22, 23, 24, 25, 26, 27};
	// Caco demon Idle
	LoadTexture(28, npc_path + "caco_demon/idle/0.png");
	LoadTexture(29, npc_path + "caco_demon/idle/1.png");
	LoadTexture(30, npc_path + "caco_demon/idle/2.png");
	LoadTexture(31, npc_path + "caco_demon/idle/3.png");
	LoadTexture(32, npc_path + "caco_demon/idle/4.png");
	LoadTexture(33, npc_path + "caco_demon/idle/5.png");
	LoadTexture(34, npc_path + "caco_demon/idle/6.png");
	LoadTexture(35, npc_path + "caco_demon/idle/7.png");
	texture_collections_["caco_demon_idle"] = {28, 29, 30, 31, 32, 33, 34, 35};
	// Caco demon Pain
	LoadTexture(36, npc_path + "caco_demon/pain/0.png");
	LoadTexture(37, npc_path + "caco_demon/pain/1.png");
	texture_collections_["caco_demon_pain"] = {36, 37};
	// Caco demon Walk
	LoadTexture(38, npc_path + "caco_demon/walk/0.png");
	LoadTexture(39, npc_path + "caco_demon/walk/1.png");
	LoadTexture(40, npc_path + "caco_demon/walk/2.png");
	texture_collections_["caco_demon_walk"] = {38, 39, 40};

	// Cyber demon Attack
	LoadTexture(41, npc_path + "cyber_demon/attack/0.png");
	LoadTexture(42, npc_path + "cyber_demon/attack/1.png");
	texture_collections_["cyber_demon_attack"] = {41, 42};
	// Cyber demon Death
	LoadTexture(43, npc_path + "cyber_demon/death/0.png");
	LoadTexture(44, npc_path + "cyber_demon/death/1.png");
	LoadTexture(45, npc_path + "cyber_demon/death/2.png");
	LoadTexture(46, npc_path + "cyber_demon/death/3.png");
	LoadTexture(47, npc_path + "cyber_demon/death/4.png");
	LoadTexture(48, npc_path + "cyber_demon/death/5.png");
	LoadTexture(49, npc_path + "cyber_demon/death/6.png");
	LoadTexture(50, npc_path + "cyber_demon/death/7.png");
	LoadTexture(51, npc_path + "cyber_demon/death/8.png");
	texture_collections_["cyber_demon_death"] = {43, 44, 45, 46, 47,
												 48, 49, 50, 51};
	// Cyber demon Idle
	LoadTexture(52, npc_path + "cyber_demon/idle/0.png");
	LoadTexture(53, npc_path + "cyber_demon/idle/1.png");
	LoadTexture(54, npc_path + "cyber_demon/idle/2.png");
	LoadTexture(55, npc_path + "cyber_demon/idle/3.png");
	LoadTexture(56, npc_path + "cyber_demon/idle/4.png");
	LoadTexture(57, npc_path + "cyber_demon/idle/5.png");
	LoadTexture(58, npc_path + "cyber_demon/idle/6.png");
	LoadTexture(59, npc_path + "cyber_demon/idle/7.png");
	texture_collections_["cyber_demon_idle"] = {52, 53, 54, 55, 56, 57, 58, 59};
	// Cyber demon Pain
	LoadTexture(60, npc_path + "cyber_demon/pain/0.png");
	LoadTexture(61, npc_path + "cyber_demon/pain/1.png");
	texture_collections_["cyber_demon_pain"] = {60, 61};
	// Cyber demon Walk
	LoadTexture(62, npc_path + "cyber_demon/walk/0.png");
	LoadTexture(63, npc_path + "cyber_demon/walk/1.png");
	LoadTexture(64, npc_path + "cyber_demon/walk/3.png");
	LoadTexture(65, npc_path + "cyber_demon/walk/4.png");
	texture_collections_["cyber_demon_walk"] = {62, 63, 64, 65};

	// Soldier Attack
	LoadTexture(66, npc_path + "soldier/attack/0.png");
	LoadTexture(67, npc_path + "soldier/attack/1.png");
	texture_collections_["soldier_attack"] = {66, 67};
	// Soldier Death
	LoadTexture(68, npc_path + "soldier/death/0.png");
	LoadTexture(69, npc_path + "soldier/death/1.png");
	LoadTexture(70, npc_path + "soldier/death/2.png");
	LoadTexture(71, npc_path + "soldier/death/3.png");
	LoadTexture(72, npc_path + "soldier/death/4.png");
	LoadTexture(73, npc_path + "soldier/death/5.png");
	LoadTexture(74, npc_path + "soldier/death/6.png");
	LoadTexture(75, npc_path + "soldier/death/7.png");
	LoadTexture(76, npc_path + "soldier/death/8.png");
	texture_collections_["soldier_death"] = {68, 69, 70, 71, 72,
											 73, 74, 75, 76};
	// Soldier Idle
	LoadTexture(77, npc_path + "soldier/idle/0.png");
	LoadTexture(78, npc_path + "soldier/idle/1.png");
	LoadTexture(79, npc_path + "soldier/idle/2.png");
	LoadTexture(80, npc_path + "soldier/idle/3.png");
	LoadTexture(81, npc_path + "soldier/idle/4.png");
	LoadTexture(82, npc_path + "soldier/idle/5.png");
	LoadTexture(83, npc_path + "soldier/idle/6.png");
	LoadTexture(84, npc_path + "soldier/idle/7.png");
	texture_collections_["soldier_idle"] = {77, 78, 79, 80, 81, 82, 83, 84};
	// Soldier Pain
	LoadTexture(85, npc_path + "soldier/pain/0.png");
	texture_collections_["soldier_pain"] = {85};
	// Soldier Walk
	LoadTexture(86, npc_path + "soldier/walk/0.png");
	LoadTexture(87, npc_path + "soldier/walk/1.png");
	LoadTexture(88, npc_path + "soldier/walk/2.png");
	LoadTexture(89, npc_path + "soldier/walk/3.png");
	texture_collections_["soldier_walk"] = {86, 87, 88, 89};
}

void TextureManager::LoadWeaponTextures() {
	const auto weapon_path = std::string(RESOURCE_DIR) + "sprites/weapon/";
	// MP5 sprites
	// Loaded
	LoadTexture(90, weapon_path + "/mp5/loaded/" + "0.png");
	LoadTexture(91, weapon_path + "/mp5/loaded/" + "1.png");
	LoadTexture(92, weapon_path + "/mp5/loaded/" + "2.png");
	LoadTexture(93, weapon_path + "/mp5/loaded/" + "3.png");
	LoadTexture(94, weapon_path + "/mp5/loaded/" + "4.png");
	LoadTexture(95, weapon_path + "/mp5/loaded/" + "5.png");
	texture_collections_["mp5_loaded"] = {90, 91, 92, 93, 94, 95};
	// outofammo
	LoadTexture(96, weapon_path + "/mp5/outofammo/" + "0.png");
	LoadTexture(97, weapon_path + "/mp5/outofammo/" + "1.png");
	LoadTexture(98, weapon_path + "/mp5/outofammo/" + "2.png");
	LoadTexture(99, weapon_path + "/mp5/outofammo/" + "3.png");
	texture_collections_["mp5_outofammo"] = {96, 97, 98, 99};
	// reload
	LoadTexture(100, weapon_path + "/mp5/reload/" + "0.png");
	LoadTexture(101, weapon_path + "/mp5/reload/" + "1.png");
	LoadTexture(102, weapon_path + "/mp5/reload/" + "2.png");
	LoadTexture(103, weapon_path + "/mp5/reload/" + "3.png");
	LoadTexture(104, weapon_path + "/mp5/reload/" + "4.png");
	LoadTexture(105, weapon_path + "/mp5/reload/" + "5.png");
	LoadTexture(106, weapon_path + "/mp5/reload/" + "6.png");
	LoadTexture(107, weapon_path + "/mp5/reload/" + "7.png");
	LoadTexture(108, weapon_path + "/mp5/reload/" + "8.png");
	LoadTexture(109, weapon_path + "/mp5/reload/" + "9.png");
	LoadTexture(110, weapon_path + "/mp5/reload/" + "10.png");
	LoadTexture(111, weapon_path + "/mp5/reload/" + "11.png");
	LoadTexture(112, weapon_path + "/mp5/reload/" + "12.png");
	LoadTexture(113, weapon_path + "/mp5/reload/" + "13.png");
	LoadTexture(114, weapon_path + "/mp5/reload/" + "14.png");
	LoadTexture(115, weapon_path + "/mp5/reload/" + "15.png");
	texture_collections_["mp5_reload"] = {100, 101, 102, 103, 104, 105,
										  106, 107, 108, 109, 110, 111,
										  112, 113, 114, 115};
}

}  // namespace wolfenstein
