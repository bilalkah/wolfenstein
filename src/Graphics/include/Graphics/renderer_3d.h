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
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <tuple>
#include <vector>

namespace wolfenstein {

class Renderer3D : public IRenderer
{
  public:
	explicit Renderer3D(std::shared_ptr<RendererContext> context);
	void RenderScene(double delta_time) override;

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
	void RenderHUD(double delta_time);
	void RenderFps(double delta_time);

	// Reused every frame: clear() keeps the capacity, so after the first
	// frame queueing never allocates
	std::vector<RenderCommand> render_queue_;

	// The FPS counter is drawn from the digits 0-9, rasterised once here:
	// a changing value costs no text rendering, texture or allocation
	struct Glyph
	{
		std::unique_ptr<SDL_Texture, TextureDeleter> texture;
		int width = 0;
		int height = 0;
	};
	std::array<Glyph, 10> fps_digits_;
	// Textures looked up by name once, when the renderer is built
	std::span<const std::uint16_t> hud_digits_;
	int sky_texture_ = 0;
	int far_texture_ = 0;  // where no wall is in view
	int crosshair_texture_ = 0;
	int damage_texture_ = 0;
	int shown_fps_ = 0;
	double fps_elapsed_ = 0.0;
	int fps_frames_ = 0;
};	// class Renderer3D

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_3D_H_
