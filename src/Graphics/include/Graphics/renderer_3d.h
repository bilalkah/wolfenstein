/**
 * @file renderer_3d.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_

#include "Graphics/renderer_interface.h"
#include <queue>
#include <tuple>
#include <vector>

namespace wolfenstein {

namespace {

struct RenderableTexture
{
	RenderableTexture(const int texture_id_, const SDL_Rect& src_rect_,
					  const SDL_Rect& dest_rect_, const double distance_)
		: texture_id(texture_id_),
		  src_rect(src_rect_),
		  dest_rect(dest_rect_),
		  distance(distance_) {}

	int texture_id;
	SDL_Rect src_rect;
	SDL_Rect dest_rect;
	double distance;
};

auto Compare{[](const RenderableTexture lhs, const RenderableTexture rhs) {
	return lhs.distance < rhs.distance;
}};

}  // namespace

class Renderer3D : public IRenderer
{
	typedef std::priority_queue<
		RenderableTexture, std::vector<RenderableTexture>, decltype(Compare)>
		RenderQueue;

  public:
	using IRenderer::IRenderer;
	~Renderer3D() = default;
	void RenderScene() override;

  private:
	void RenderBackground();
	void RenderWalls(RenderQueue& render_queue);
	void RenderIfRayHit(const int& horizontal_slice, const Ray& ray,
						RenderQueue& render_queue);
	void RenderIfRayHitNot(const int& horizontal_slice,
						   RenderQueue& render_queue);
	void RenderObjects(RenderQueue& render_queue);
	int CalculateHorizontalSlice(const double& angle);
	std::tuple<int, int, int> CalculateVerticalSlice(const double& distance);
	void RenderWeapon(RenderQueue& render_queue);
	void RenderTextures(RenderQueue& render_queue);
	void RenderHUD();
};	// class Renderer3D

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_
