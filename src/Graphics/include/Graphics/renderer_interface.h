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
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>

namespace karakale {

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
// across, where a flat camera plane a unit ahead is 2 tan(fov / 2) wide
inline double PixelsPerUnit(const RenderConfig& config, double fov) {
	return config.height * std::tan(config.base_fov / 2) / std::tan(fov / 2);
}

// Screen pixels the middle of the picture moves for a radian of turn, in a
// view `fov` radians across (towards the edges a radian spans more)
inline double PixelsPerRadian(const RenderConfig& config, double fov) {
	return config.width / (2 * std::tan(fov / 2));
}

// SDL draws at fractional positions; the views lay out in whole pixels
inline SDL_FRect ToFRect(const SDL_Rect& rect) {
	SDL_FRect result{};
	SDL_RectToFRect(&rect, &result);
	return result;
}

// Cuts a picture drawn over `dest` down to the part inside `screen`: `dest`
// becomes that part and `src` the part of the picture it shows (counted from
// the other side if the picture is drawn mirrored). False if none of it
// shows. A sprite beside the eye can reach far past the screen's sides, and
// a renderer that draws in software (as the tests' does) first makes an
// image the size of the whole rectangle: uncut, one ran out of memory.
inline bool ClipToScreen(SDL_Rect& src, SDL_Rect& dest, const SDL_Rect& screen,
						 bool mirrored) {
	SDL_Rect visible;
	if (src.w <= 0 || src.h <= 0 ||
		!SDL_GetRectIntersection(&dest, &screen, &visible)) {
		return false;
	}
	// Cut at whole texels, so the picture keeps its scale: the rectangle
	// ends at most a texel past the screen's edge
	const double across = static_cast<double>(dest.w) / src.w;
	const double up = static_cast<double>(dest.h) / src.h;
	const int cut_left = visible.x - dest.x;
	const int cut_right = dest.x + dest.w - visible.x - visible.w;
	const int cut_top = visible.y - dest.y;
	const int cut_bottom = dest.y + dest.h - visible.y - visible.h;
	const int first =
		static_cast<int>((mirrored ? cut_right : cut_left) / across);
	const int last =
		src.w - static_cast<int>((mirrored ? cut_left : cut_right) / across);
	const int top = static_cast<int>(cut_top / up);
	const int bottom = src.h - static_cast<int>(cut_bottom / up);
	const double left = dest.x + (mirrored ? src.w - last : first) * across;
	const double upper = dest.y + top * up;
	const SDL_Rect whole{static_cast<int>(std::floor(left)),
						 static_cast<int>(std::floor(upper)),
						 static_cast<int>(std::ceil((last - first) * across)),
						 static_cast<int>(std::ceil((bottom - top) * up))};
	// A texel wider than the margin (a sprite right beside the eye) is drawn
	// over the screen and the margin only: there the picture is a patch or
	// two of one colour
	constexpr int kMargin = 64;
	const SDL_Rect bound{screen.x - kMargin, screen.y - kMargin,
						 screen.w + 2 * kMargin, screen.h + 2 * kMargin};
	SDL_GetRectIntersection(&whole, &bound, &dest);
	src = {src.x + first, src.y + top, last - first, bottom - top};
	return true;
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

}  // namespace karakale

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_INTERFACE_H_
