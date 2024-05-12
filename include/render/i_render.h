/**
 * @file i_render.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Render interface for rendering objects on screen
 * @version 0.1
 * @date 2024-05-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef RENDER_I_RENDER_H
#define RENDER_I_RENDER_H

#include "base/types.h"
#include <SDL2/SDL.h>
#include <SDL_mutex.h>
#include <memory>
#include <vector>

namespace wolfenstein {

class ICharacter;

class IRender
{
  public:
	virtual ~IRender() = default;
	virtual void Render(std::vector<std::shared_ptr<ICharacter>>) = 0;
};

class CommonRender : public IRender
{
  public:
	CommonRender(SDL_Renderer* renderer);
	virtual ~CommonRender() = default;
	void ClearScreen();
	void SetDrawColor(SDL_Color color);

	void Render(std::vector<std::shared_ptr<ICharacter>> characters) override;

  protected:
	SDL_Window* window_;
	SDL_Renderer* renderer_;
};

class SimpleRender : public CommonRender
{
  public:
	SimpleRender(SDL_Renderer* renderer);
	virtual ~SimpleRender() = default;

	void DrawPixel(vector2i point);
	void DrawLine(vector2i start, vector2i end);
	void DrawCircle(vector2i center, int radius);
	void DrawRectangle(vector2i start, vector2i end);
	void DrawFilledRectangle(vector2i start, vector2i end);
};

}  // namespace wolfenstein

#endif	// RENDER_I_RENDER_H
