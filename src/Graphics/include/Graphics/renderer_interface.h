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
#include <memory>
#include <optional>

namespace wolfenstein {

struct RenderConfig
{
	RenderConfig(int width, int height, int padding, int scale, int fps,
				 double view_distance, double fov, bool fullscreen)
		: width(width),
		  height(height),
		  padding(padding),
		  scale(scale),
		  fps(fps),
		  view_distance(view_distance),
		  fov(fov),
		  fullscreen(fullscreen) {}
	int width;
	int height;
	int padding;
	int scale;
	int fps;
	double view_distance;
	double fov;
	bool fullscreen;
};

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
	RenderConfig GetConfig() const;
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
	explicit IRenderer(std::shared_ptr<RendererContext> context);
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

	std::shared_ptr<RendererContext> GetContext() const;

	std::shared_ptr<RendererContext> context_;
	Scene* scene_ = nullptr;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_INTERFACE_H_
