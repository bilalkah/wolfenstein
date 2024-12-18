#include "Graphics/renderer_result.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <SDL2/SDL_render.h>

namespace wolfenstein {

RendererResult::RendererResult(std::shared_ptr<RendererContext> context,
							   const uint16_t texture_id)
	: context_(context),
	  result_animation_(std::make_unique<TriggeredSingleAnimation>(
		  texture_id, 0.2, 0, 255)) {}

void RendererResult::Render() {
	ClearScreen();
	RenderScreen();
	SDL_RenderPresent(context_->GetRenderer());
}

void RendererResult::RenderScreen() {
	result_animation_->Update(TimeManager::GetInstance().GetDeltaTime());

	SDL_RenderCopy(context_->GetRenderer(),
				   TextureManager::GetInstance()
					   .GetTexture(result_animation_->GetCurrentFrame())
					   .texture,
				   nullptr, nullptr);
}

void RendererResult::ClearScreen() {
	SDL_SetRenderDrawColor(context_->GetRenderer(), 0, 0, 0, 255);
	SDL_RenderClear(context_->GetRenderer());
}

}  // namespace wolfenstein
