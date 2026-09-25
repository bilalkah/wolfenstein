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

#include "Graphics/renderer_interface.h"
#include <SDL2/SDL.h>
#include <cstddef>
#include <vector>

namespace wolfenstein {

class Renderer2D : public IRenderer
{
  public:
	explicit Renderer2D(RendererContext& context);
	void RenderScene(double delta_time) override;

	// The most vertices one draw call carries; startup grows SDL's vertex
	// buffer to at least this
	static constexpr std::size_t kVertexCapacity = 16384;

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
	void AddQuad(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FPoint d);
	void Flush();

	// Everything the view draws in a frame is collected here and drawn in
	// one SDL_RenderGeometry call: SDL's own point and line functions build
	// temporary arrays on the heap. Sized once; a full batch is drawn and
	// the next one reuses the storage.
	std::vector<SDL_Vertex> vertices_;
	std::vector<int> indices_;
	SDL_Color color_{255, 255, 255, 255};

};	// class Renderer2D

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_2D_H_
