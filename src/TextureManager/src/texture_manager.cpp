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

	// Static textures
	std::string texture_path = std::string(RESOURCE_DIR) + "textures/";
	LoadTexture(0, texture_path + "sky.png");
	LoadTexture(1, texture_path + "1.png");
	LoadTexture(2, texture_path + "2.png");
	LoadTexture(3, texture_path + "3.png");
	LoadTexture(4, texture_path + "4.png");
	LoadTexture(5, texture_path + "5.png");
	LoadTexture(7, texture_path + "solid_black.png");

	// Static sprites
	std::string sprite_path = std::string(RESOURCE_DIR) + "sprites/";
	LoadTexture(8, sprite_path + "static_sprites/candlebra.png");

	// Animated sprites
	LoadTexture(9, sprite_path + "animated_sprites/green_light/0.png");
	LoadTexture(10, sprite_path + "animated_sprites/green_light/1.png");
	LoadTexture(11, sprite_path + "animated_sprites/green_light/2.png");
	LoadTexture(12, sprite_path + "animated_sprites/green_light/3.png");
	LoadTexture(13, sprite_path + "animated_sprites/red_light/0.png");
	LoadTexture(14, sprite_path + "animated_sprites/red_light/1.png");
	LoadTexture(15, sprite_path + "animated_sprites/red_light/2.png");
	LoadTexture(16, sprite_path + "animated_sprites/red_light/3.png");

	// Npc sprites
	std::string npc_path = std::string(RESOURCE_DIR) + "sprites/npc/";
	// Caco demon Attack
	LoadTexture(17, npc_path + "caco_demon/attack/0.png");
	LoadTexture(18, npc_path + "caco_demon/attack/1.png");
	LoadTexture(19, npc_path + "caco_demon/attack/2.png");
	LoadTexture(20, npc_path + "caco_demon/attack/3.png");
	LoadTexture(21, npc_path + "caco_demon/attack/4.png");
	// Caco demon Death
	LoadTexture(22, npc_path + "caco_demon/death/0.png");
	LoadTexture(23, npc_path + "caco_demon/death/1.png");
	LoadTexture(24, npc_path + "caco_demon/death/2.png");
	LoadTexture(25, npc_path + "caco_demon/death/3.png");
	LoadTexture(26, npc_path + "caco_demon/death/4.png");
	LoadTexture(27, npc_path + "caco_demon/death/5.png");
	// Caco demon Idle
	LoadTexture(28, npc_path + "caco_demon/idle/0.png");
	LoadTexture(29, npc_path + "caco_demon/idle/1.png");
	LoadTexture(30, npc_path + "caco_demon/idle/2.png");
	LoadTexture(31, npc_path + "caco_demon/idle/3.png");
	LoadTexture(32, npc_path + "caco_demon/idle/4.png");
	LoadTexture(33, npc_path + "caco_demon/idle/5.png");
	LoadTexture(34, npc_path + "caco_demon/idle/6.png");
	LoadTexture(35, npc_path + "caco_demon/idle/7.png");
	// Caco demon Pain
	LoadTexture(36, npc_path + "caco_demon/pain/0.png");
	LoadTexture(37, npc_path + "caco_demon/pain/1.png");
	// Caco demon Walk
	LoadTexture(38, npc_path + "caco_demon/walk/0.png");
	LoadTexture(39, npc_path + "caco_demon/walk/1.png");
	LoadTexture(40, npc_path + "caco_demon/walk/2.png");

	// Cyber demon Attack
	LoadTexture(41, npc_path + "cyber_demon/attack/0.png");
	LoadTexture(42, npc_path + "cyber_demon/attack/1.png");

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

	// Cyber demon Idle
	LoadTexture(52, npc_path + "cyber_demon/idle/0.png");
	LoadTexture(53, npc_path + "cyber_demon/idle/1.png");
	LoadTexture(54, npc_path + "cyber_demon/idle/2.png");
	LoadTexture(55, npc_path + "cyber_demon/idle/3.png");
	LoadTexture(56, npc_path + "cyber_demon/idle/4.png");
	LoadTexture(57, npc_path + "cyber_demon/idle/5.png");
	LoadTexture(58, npc_path + "cyber_demon/idle/6.png");
	LoadTexture(59, npc_path + "cyber_demon/idle/7.png");

	// Cyber demon Pain
	LoadTexture(60, npc_path + "cyber_demon/pain/0.png");
	LoadTexture(61, npc_path + "cyber_demon/pain/1.png");

	// Cyber demon Walk
	LoadTexture(62, npc_path + "cyber_demon/walk/0.png");
	LoadTexture(63, npc_path + "cyber_demon/walk/1.png");
	LoadTexture(64, npc_path + "cyber_demon/walk/2.png");
	LoadTexture(65, npc_path + "cyber_demon/walk/3.png");

	// Soldier Attack
	LoadTexture(66, npc_path + "soldier/attack/0.png");
	LoadTexture(67, npc_path + "soldier/attack/1.png");

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

	// Soldier Idle
	LoadTexture(77, npc_path + "soldier/idle/0.png");
	LoadTexture(78, npc_path + "soldier/idle/1.png");
	LoadTexture(79, npc_path + "soldier/idle/2.png");
	LoadTexture(80, npc_path + "soldier/idle/3.png");
	LoadTexture(81, npc_path + "soldier/idle/4.png");
	LoadTexture(82, npc_path + "soldier/idle/5.png");
	LoadTexture(83, npc_path + "soldier/idle/6.png");
	LoadTexture(84, npc_path + "soldier/idle/7.png");

	// Soldier Pain
	LoadTexture(85, npc_path + "soldier/pain/0.png");

	// Soldier Walk
	LoadTexture(86, npc_path + "soldier/walk/0.png");
	LoadTexture(87, npc_path + "soldier/walk/1.png");
	LoadTexture(88, npc_path + "soldier/walk/2.png");
	LoadTexture(89, npc_path + "soldier/walk/3.png");

	// Weapon sprites
	std::string weapon_path = std::string(RESOURCE_DIR) + "sprites/weapon/";
	LoadTexture(90, weapon_path + "shotgun/0.png");
	LoadTexture(91, weapon_path + "shotgun/1.png");
	LoadTexture(92, weapon_path + "shotgun/2.png");
	LoadTexture(93, weapon_path + "shotgun/3.png");
	LoadTexture(94, weapon_path + "shotgun/4.png");
	LoadTexture(95, weapon_path + "shotgun/5.png");
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

}  // namespace wolfenstein
