/**
 * @file render_3d_simple.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-13
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef RENDER_RENDER_3D_SIMPLE_H
#define RENDER_RENDER_3D_SIMPLE_H

#include "character/i_character.h"
#include "map/map.h"
#include "render/i_render.h"

#include <SDL2/SDL.h>
#include <map>
#include <memory>
#include <vector>

namespace wolfenstein {
class Render3DSimple : public IRender
{
  public:
	Render3DSimple(RenderConfig config);
	~Render3DSimple();
	void Render(std::vector<std::shared_ptr<ICharacter>>) override;
	void SetMap(std::shared_ptr<wolfenstein::Map> map);

  private:
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
	std::map<int, SDL_Color> color_map_;
};
}  // namespace wolfenstein

#endif	// RENDER_RENDER_3D_SIMPLE_H
