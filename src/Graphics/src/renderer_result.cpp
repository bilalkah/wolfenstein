#include "Graphics/renderer_result.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL2/SDL_render.h>

namespace wolfenstein {

RendererResult::RendererResult(std::shared_ptr<RendererContext> context,
							   Texture texture)
	: context_(context), texture_(texture) {}

void RendererResult::Render() {
	ClearScreen();
	RenderScreen();
	SDL_RenderPresent(context_->GetRenderer());
}

void RendererResult::RenderScreen() {
	fade_counter_ += TimeManager::GetInstance().GetDeltaTime() / 5.0;
	if (fade_counter_ >= 1.0) {
		fade_counter_ = 1.0;
	}
	SDL_SetTextureAlphaMod(texture_.texture,
						   static_cast<int>(fade_counter_ * 255));
	SDL_Rect src_rect = {0, 0, texture_.width, texture_.height};
	SDL_Rect dest_rect = {0, 0, context_->GetConfig().width,
						  context_->GetConfig().height};
	SDL_RenderCopy(context_->GetRenderer(), texture_.texture, &src_rect,
				   &dest_rect);
}

void RendererResult::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

}  // namespace wolfenstein
