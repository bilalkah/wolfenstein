#include "Graphics/renderer_result.h"
#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_render.h>

namespace wolfenstein {

RendererResult::RendererResult(std::shared_ptr<RendererContext> context,
							   const uint16_t texture_id)
	: context_(context), result_animation_(texture_id, 0.2, 0, 255) {}

void RendererResult::Render(double delta_time) {
	ClearScreen();
	RenderScreen(delta_time);
}

void RendererResult::RenderScreen(double delta_time) {
	result_animation_.Update(delta_time);

	SDL_Texture* texture = context_->Textures()
							   .GetTexture(static_cast<std::uint16_t>(
								   result_animation_.GetCurrentFrame()))
							   .texture;
	SDL_SetTextureAlphaMod(texture, result_animation_.GetAlpha());
	SDL_RenderCopy(context_->GetRenderer(), texture, nullptr, nullptr);
}

void RendererResult::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

}  // namespace wolfenstein
