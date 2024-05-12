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

#include <memory>
#include <vector>

namespace wolfenstein {

class ICharacter;

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

class IRender
{
  public:
	virtual ~IRender() = default;
	virtual void Render(std::vector<std::shared_ptr<ICharacter>>) = 0;
};

}  // namespace wolfenstein

#endif	// RENDER_I_RENDER_H
