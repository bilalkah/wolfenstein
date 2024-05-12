/**
 * @file render_2d.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 2D render class for rendering objects on screen
 * @version 0.1
 * @date 2024-05-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef RENDER_RENDER_2D_H
#define RENDER_RENDER_2D_H

#include "character/i_character.h"
#include "map/map.h"
#include "render/i_render.h"
#include <SDL2/SDL.h>
#include <memory>
#include <vector>

namespace wolfenstein {

struct RenderConfig
{
	int width;
	int height;
	int padding;
	int scale;
	int fps;
	double view_distance;
	double fov;
};

class Render2D final : public IRender
{
  public:
	Render2D(RenderConfig config);
	~Render2D();

	void Render(std::vector<std::shared_ptr<ICharacter>> characters) override;

	void SetMap(std::shared_ptr<wolfenstein::Map> map);

  private:
	void RenderMap();
	void RenderPlayer(std::vector<std::shared_ptr<ICharacter>> characters);

	void ClearScreen();
	void SetDrawColor(SDL_Color color);
	void DrawPixel(vector2i point);
	void DrawLine(vector2i start, vector2i end);
	void DrawCircle(vector2i center, int radius);
	void DrawRectangle(vector2i start, vector2i end);
	void DrawFilledRectangle(vector2i start, vector2i end);

	SDL_Window* window_;
	SDL_Renderer* renderer_;

	std::shared_ptr<wolfenstein::Map> map_;
	RenderConfig config_;
};

}  // namespace wolfenstein

#endif	// RENDER_RENDER_2D_H
