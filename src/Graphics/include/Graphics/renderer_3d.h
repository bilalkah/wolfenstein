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
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

namespace wolfenstein {

class Renderer3D : public IRenderer
{
  public:
	explicit Renderer3D(std::shared_ptr<RendererContext> context);
	void RenderScene() override;

  private:
	// One textured rectangle to draw. The queue is drawn back to front;
	// `order` keeps equal distances in submission order, deterministically
	struct RenderCommand
	{
		int texture_id;
		SDL_Rect src_rect;
		SDL_Rect dest_rect;
		double distance;
		std::uint32_t order;
	};

	struct TextureDeleter
	{
		void operator()(SDL_Texture* texture) const noexcept;
	};

	void Enqueue(int texture_id, const SDL_Rect& src_rect,
				 const SDL_Rect& dest_rect, double distance);
	void RenderBackground();
	void RenderWalls();
	void RenderIfRayHit(const int& horizontal_slice, const Ray& ray);
	void RenderIfRayHitNot(const int& horizontal_slice);
	void RenderObjects();
	int CalculateHorizontalSlice(const double& angle);
	std::tuple<int, int, int> CalculateVerticalSlice(const double& distance);
	void RenderWeapon();
	void RenderTextures();
	void RenderHUD();
	void RenderFps();

	// Reused every frame: clear() keeps the capacity, so after the first
	// frame queueing never allocates
	std::vector<RenderCommand> render_queue_;

	// The FPS counter is rasterised into a texture only when its value
	// changes, at most every kFpsRefreshSeconds, instead of every frame
	std::unique_ptr<SDL_Texture, TextureDeleter> fps_texture_;
	SDL_Rect fps_rect_{};
	int shown_fps_ = -1;
	double fps_elapsed_ = 0.0;
	int fps_frames_ = 0;
};	// class Renderer3D

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_
