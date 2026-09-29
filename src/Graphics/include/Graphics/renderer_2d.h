/**
 * @file renderer_2d.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_2D_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_2D_H_

#include "Graphics/quad_batch.h"
#include "Graphics/renderer_interface.h"
#include <SDL3/SDL.h>
#include <cstddef>
#include <vector>

namespace wolfenstein {

class Renderer2D : public IRenderer
{
  public:
	explicit Renderer2D(RendererContext& context);
	void RenderScene(double delta_time) override;

	static constexpr std::size_t kVertexCapacity = QuadBatch::kVertexCapacity;

  private:
	void RenderMap();
	void RenderPlayer();
	void RenderObjects();
	void RenderPaths();
	void RenderCrosshairs();

	void SetDrawColor(SDL_Color color);
	void DrawFilledRectangle(vector2i start, vector2i end);
	void DrawLine(vector2i start, vector2i end);
	void DrawCircle(vector2i center, int radius);

	// Everything the view draws in a frame, drawn in one call
	QuadBatch batch_;

};	// class Renderer2D

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_2D_H_
