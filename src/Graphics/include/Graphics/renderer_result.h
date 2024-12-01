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

#include "Graphics/renderer_interface.h"
#include "TextureManager/texture_manager.h"
#include <SDL2/SDL_render.h>
#include <memory>

namespace wolfenstein {

class RendererResult
{
  public:
	RendererResult(std::shared_ptr<RendererContext> context, Texture texture);
	~RendererResult() = default;

	void Render();

  private:
	void ClearScreen();
	void RenderScreen();
	std::shared_ptr<RendererContext> context_;
	Texture texture_;
	double fade_counter_{0.0};
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_RESULT_H_
