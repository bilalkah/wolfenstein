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

namespace wolfenstein {

class Renderer2D : public IRenderer
{
  public:
	using IRenderer::IRenderer;
	~Renderer2D() = default;
	void RenderScene(const std::shared_ptr<Scene>& scene_ptr) override;

  private:
	void RenderMap(const std::shared_ptr<Map> map_ptr);
	void RenderPlayer(const std::shared_ptr<Player> player_ptr);
	void RenderObjects(
		const std::vector<std::shared_ptr<IGameObject>>& objects);
	void RenderPaths(const std::vector<std::shared_ptr<Enemy>>& enemies);
	void RenderCrosshairs(const std::vector<std::shared_ptr<Enemy>>& enemies);

	void SetDrawColor(SDL_Color color);
	void DrawFilledRectangle(vector2i start, vector2i end);
	void DrawLine(vector2i start, vector2i end);

};	// class Renderer2D

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_2D_H_
