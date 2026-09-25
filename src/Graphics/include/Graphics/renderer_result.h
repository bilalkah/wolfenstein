/**
 * @file renderer_result.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_RESULT_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_RESULT_H_

#include "Animation/triggered_single_animation.h"
#include "Graphics/renderer_interface.h"
#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_render.h>
#include <memory>

namespace wolfenstein {

class RendererResult
{
  public:
	// Fades in the texture with the given id (the win or game over screen)
	RendererResult(RendererContext& context, int texture_id);

	void Render(double delta_time);

  private:
	void ClearScreen();
	void RenderScreen(double delta_time);
	RendererContext* context_;
	int texture_id_;
	TriggeredSingleAnimation result_animation_;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_RESULT_H_
