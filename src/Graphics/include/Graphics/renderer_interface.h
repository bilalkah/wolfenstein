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
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>

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
	SDL_Renderer* GetRenderer() const;
	TTF_Font* GetFont() const;
	SDL_Window* GetWindow() const;
	RenderConfig GetConfig() const;
	const Camera2D& GetCamera() const;
	Camera2D& GetCamera();

  private:
	SDL_Renderer* renderer_;
	TTF_Font* font_;
	SDL_Window* window_;
	RenderConfig config_;
	Camera2D& camera_ptr;
};

class IRenderer
{
  public:
	IRenderer(std::shared_ptr<RendererContext> context);
	virtual ~IRenderer() = default;

	virtual void RenderScene() = 0;
	void SetScene(const std::shared_ptr<Scene>& scene_ptr);

  protected:
	void ClearScreen();

	std::shared_ptr<RendererContext> GetContext() const;

	std::shared_ptr<RendererContext> context_;
	std::shared_ptr<Scene> scene_;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_INTERFACE_H_
