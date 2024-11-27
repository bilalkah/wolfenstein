/**
 * @file renderer.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-29
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_H_

#include "Camera/camera.h"
#include "Characters/enemy.h"
#include "Core/scene.h"
#include "GameObjects/game_object.h"
#include <SDL2/SDL.h>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

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

class Renderer
{
	typedef std::priority_queue<
		RenderableTexture, std::vector<RenderableTexture>, decltype(Compare)>
		RenderQueue;

  public:
	explicit Renderer(const std::string& window_name,
					  const RenderConfig& config,
					  std::shared_ptr<Camera2D>& camera);
	~Renderer();

	void RenderScene(const std::shared_ptr<Scene>& scene_ptr);

	void RenderScene2D(const std::shared_ptr<Scene>& scene_ptr);

  private:
	void RenderBackground();
	void RenderWalls(const std::shared_ptr<Map>& map_ptr,
					 RenderQueue& render_queue);
	void RenderIfRayHit(const int horizontal_slice, const Ray& ray,
						RenderQueue& render_queue);
	void RenderIfRayHitNot(const int horizontal_slice,
						   RenderQueue& render_queue);
	void RenderObjects(const std::vector<std::shared_ptr<IGameObject>>& objects,
					   RenderQueue& render_queue);
	int CalculateHorizontalSlice(const double& angle);
	std::tuple<int, int, int> CalculateVerticalSlice(const double& distance);
	void RenderWeapon(const std::shared_ptr<Player>& player_ptr,
					  RenderQueue& render_queue);
	void RenderTextures(RenderQueue& render_queue);

	// Render 2D
	void ClearScreen();
	void SetDrawColor(SDL_Color color);
	void DrawFilledRectangle(vector2i start, vector2i end);
	void RenderMap(const std::shared_ptr<Map> map_ptr);
	void RenderPlayer(const std::shared_ptr<Player> player_ptr);
	void RenderObjects(
		const std::vector<std::shared_ptr<IGameObject>>& objects);
	void RenderPaths(const std::vector<std::shared_ptr<Enemy>>& enemies);
	void RenderCrosshair(const std::vector<std::shared_ptr<Enemy>>& enemies);
	void DrawLine(vector2i start, vector2i end);

	SDL_Renderer* renderer_;
	SDL_Window* window_;
	RenderConfig config_;
	std::shared_ptr<Camera2D> camera_ptr;
};

}  // namespace wolfenstein
#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_H_
