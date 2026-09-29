/**
 * @file renderer_interface.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_INTERFACE_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_INTERFACE_H_

#include "Camera/camera.h"
#include "Core/scene.h"
#include "TextureManager/texture_manager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>

namespace wolfenstein {

struct RenderConfig
{
	RenderConfig(int width, int height, int padding, int scale, int fps,
				 double view_distance, double base_fov, bool fullscreen)
		: width(width),
		  height(height),
		  padding(padding),
		  scale(scale),
		  fps(fps),
		  view_distance(view_distance),
		  base_fov(base_fov),
		  fullscreen(fullscreen) {}
	int width;
	int height;
	int padding;
	int scale;
	int fps;
	double view_distance;
	// The view the picture is scaled for, in radians: seen across it, a wall
	// one unit away fills the screen's height. The player can widen the
	// view (the camera's), which shows everything smaller.
	double base_fov;
	bool fullscreen;
};

// Screen pixels something one unit tall spans one unit away, in a view
// `fov` radians across: a wider view shrinks it up and down as it does
// across
inline double PixelsPerUnit(const RenderConfig& config, double fov) {
	return config.height * config.base_fov / fov;
}

// How the sky's picture goes round the view: `width` pixels a repeat, a
// whole number of repeats a full turn, so it joins up wherever the player
// looks (with a part of a repeat left over, it jumped where the view's
// angle wraps round, looking one way); and where the first repeat starts,
// `offset` pixels left of the screen's edge, for a view turned `theta`
struct SkyLayout
{
	double width = 0.0;
	double offset = 0.0;
};
inline SkyLayout LaySky(double theta, double pixels_per_radian,
						double natural_width) {
	const double turn = 2.0 * std::numbers::pi * pixels_per_radian;
	const double repeats = std::max(1.0, std::round(turn / natural_width));
	const double width = turn / repeats;
	double offset = std::fmod(theta * pixels_per_radian, width);
	if (offset < 0.0) {
		offset += width;
	}
	return {.width = width, .offset = offset};
}

class RendererContext
{
  public:
	explicit RendererContext(const std::string& window_name,
							 const RenderConfig& config, Camera2D& camera);
	~RendererContext();
	// Owns the SDL window, renderer and font: a copy would destroy them twice
	RendererContext(const RendererContext&) = delete;
	RendererContext& operator=(const RendererContext&) = delete;
	RendererContext(RendererContext&&) = delete;
	RendererContext& operator=(RendererContext&&) = delete;
	SDL_Renderer* GetRenderer() const;
	TTF_Font* GetFont() const;
	SDL_Window* GetWindow() const;
	const RenderConfig& GetConfig() const { return config_; }
	const Camera2D& GetCamera() const;
	Camera2D& GetCamera();
	TextureManager& Textures() { return *textures_; }
	const TextureManager& Textures() const { return *textures_; }

  private:
	SDL_Renderer* renderer_;
	TTF_Font* font_;
	SDL_Window* window_;
	RenderConfig config_;
	Camera2D& camera_ptr;
	// Loaded once the renderer exists, destroyed before it
	std::unique_ptr<TextureManager> textures_;
};

class IRenderer
{
  public:
	// Borrows the context, which outlives every view
	explicit IRenderer(RendererContext& context);
	virtual ~IRenderer() = default;

  protected:
	// Copies and moves only through derived classes: copying through the
	// base would slice off the derived part
	IRenderer(const IRenderer&) = default;
	IRenderer& operator=(const IRenderer&) = default;
	IRenderer(IRenderer&&) = default;
	IRenderer& operator=(IRenderer&&) = default;

  public:
	// delta_time: seconds the frame took, for anything the view animates
	virtual void RenderScene(double delta_time) = 0;
	// Borrows the scene until the next call; the game owns it
	void SetScene(Scene& scene);

  protected:
	void ClearScreen();

	RendererContext* context_;
	Scene* scene_ = nullptr;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_INTERFACE_H_
