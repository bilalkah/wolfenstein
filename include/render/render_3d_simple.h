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

#include "render/i_render.h"

namespace wolfenstein {

class Render3DSimple
{
  public:
	Render3DSimple();
	~Render3DSimple();

	void Render(std::vector<std::shared_ptr<ICharacter>> characters);

  private:
    void RenderMap();
    void RenderPlayer(std::vector<std::shared_ptr<ICharacter>> characters);

    void ClearScreen();

};

}  // namespace wolfenstein

#endif	// RENDER_RENDER_3D_SIMPLE_H