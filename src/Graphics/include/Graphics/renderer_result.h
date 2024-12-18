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
	RendererResult(std::shared_ptr<RendererContext> context,
				   const uint16_t texture_id);
	~RendererResult() = default;

	void Render();

  private:
	void ClearScreen();
	void RenderScreen();
	std::shared_ptr<RendererContext> context_;
	std::unique_ptr<TriggeredSingleAnimation> result_animation_;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_RESULT_H_
