#include "Graphics/renderer_result.h"
#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_render.h>
#include <utility>

namespace wolfenstein {

RendererResult::RendererResult(RendererContext& context, int texture_id)
	: context_(&context),
	  texture_id_(texture_id),
	  result_animation_(0.2, 0, 255) {}

void RendererResult::Render(double delta_time) {
	ClearScreen();
	RenderScreen(delta_time);
}

void RendererResult::RenderScreen(double delta_time) {
	result_animation_.Update(delta_time);

	SDL_Texture* texture = context_->Textures().GetTexture(texture_id_).texture;
	SDL_SetTextureAlphaMod(texture, result_animation_.GetAlpha());
	SDL_RenderCopy(context_->GetRenderer(), texture, nullptr, nullptr);
}

void RendererResult::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

}  // namespace wolfenstein
