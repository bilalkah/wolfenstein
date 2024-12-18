#include "Graphics/renderer_menu.h"
#include "Math/vector.h"
#include "State/weapon_state.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_rect.h>
#include <X11/X.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace wolfenstein {

Menu::Menu(std::shared_ptr<RendererContext> context) : context_(context) {
	std::vector<std::string> weapon_names = {"mp5", "shotgun"};
	for (const auto& w : weapon_names) {
		weapons_.push_back(std::make_shared<Weapon>(w));
		weapons_.back()->Init();
	}
}

void Menu::Render() {
	Animate();
	ClearScreen();
	RenderBackground();
	RenderWeapon();
	RenderText();
	SDL_RenderPresent(context_->GetRenderer());
}

void Menu::RenderText() {
	SDL_Color color = {255, 255, 255, 255};	 // White text
	SDL_Surface* textSurface = TTF_RenderText_Solid(
		context_->GetFont(), "Hello Comrade, Choose your weapon below", color);
	if (!textSurface) {
		std::cerr << "Failed to create text surface: " << TTF_GetError()
				  << std::endl;
		exit(EXIT_FAILURE);
	}
	SDL_Texture* textTexture =
		SDL_CreateTextureFromSurface(context_->GetRenderer(), textSurface);
	if (!textTexture) {
		std::cerr << "Failed to create texture: " << SDL_GetError()
				  << std::endl;
	}
	SDL_FreeSurface(
		textSurface);  // Free the surface after creating the texture
	const double ratio = static_cast<double>(textSurface->h) / textSurface->w;
	const int width_slice = context_->GetConfig().width / 1.5;
	const int height_slice = width_slice * ratio;
	SDL_Rect rect = {(context_->GetConfig().width / 2) + (-width_slice / 2),
					 (context_->GetConfig().height / 2) +
						 (-context_->GetConfig().height / 3),
					 width_slice, height_slice};  // Position and size
	SDL_RenderCopy(context_->GetRenderer(), textTexture, nullptr, &rect);
	SDL_DestroyTexture(textTexture);

	SDL_Surface* textSurface2 = TTF_RenderText_Solid(
		context_->GetFont(), "Use <-, -> and space for selection", color);
	if (!textSurface2) {
		std::cerr << "Failed to create text surface: " << TTF_GetError()
				  << std::endl;
		exit(EXIT_FAILURE);
	}
	SDL_Texture* textTexture2 =
		SDL_CreateTextureFromSurface(context_->GetRenderer(), textSurface2);
	if (!textTexture2) {
		std::cerr << "Failed to create texture: " << SDL_GetError()
				  << std::endl;
	}
	SDL_FreeSurface(
		textSurface2);	// Free the surface after creating the texture
	const double ratio_2 =
		static_cast<double>(textSurface2->h) / textSurface2->w;
	const int width_slice_2 = context_->GetConfig().width / 1.5;
	const int height_slice_2 = width_slice_2 * ratio_2;
	SDL_Rect rect2 = {(context_->GetConfig().width / 2) + (-width_slice_2 / 2),
					  (context_->GetConfig().height / 2) +
						  (context_->GetConfig().height / 12),
					  width_slice_2, height_slice_2};  // Position and size
	SDL_RenderCopy(context_->GetRenderer(), textTexture2, nullptr, &rect2);
	SDL_DestroyTexture(textTexture2);
}

void Menu::RenderWeapon() {
	const auto id = weapons_[selected_weapon_index_]->GetTextureId();
	const auto texture = TextureManager::GetInstance().GetTexture(id);
	const double ratio = static_cast<double>(texture.height) / texture.width;
	const int width_slice = context_->GetConfig().width / 2.6;
	const int height_slice = width_slice * ratio;
	SDL_Rect src_rect = {0, 0, width_slice, height_slice};
	SDL_Rect dest_rect = {context_->GetConfig().width / 2 - width_slice / 2 -
							  context_->GetConfig().width / 20,
						  context_->GetConfig().height / 2 - height_slice / 2 -
							  context_->GetConfig().height / 8,
						  width_slice, height_slice};
	SDL_RenderCopy(context_->GetRenderer(), texture.texture, &src_rect,
				   &dest_rect);
}

void Menu::Animate() {
	weapons_[selected_weapon_index_]->Update(
		TimeManager::GetInstance().GetDeltaTime());
}

void Menu::ChangeSelection(int direction) {
	weapons_[selected_weapon_index_]->TransitionTo(
		std::make_shared<LoadedState>());
	selected_weapon_index_ += direction;
	selected_weapon_index_ %= weapons_.size();
	weapons_[selected_weapon_index_]->Reload();
}

std::shared_ptr<Weapon> Menu::GetSelectedWeapon() {
	weapons_[selected_weapon_index_]->TransitionTo(
		std::make_unique<LoadedState>());
	return weapons_[selected_weapon_index_];
}

void Menu::RenderBackground() {
	SDL_RenderCopy(context_->GetRenderer(),
				   TextureManager::GetInstance().GetTexture(8).texture, nullptr,
				   nullptr);
}

void Menu::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

}  // namespace wolfenstein
