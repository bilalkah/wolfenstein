/**
 * @file render_3d_texture.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-13
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef RENDER_RENDER_3D_TEXTURE_H
#define RENDER_RENDER_3D_TEXTURE_H

#include "character/i_character.h"
#include "map/map.h"
#include "render/i_render.h"

#include <SDL2/SDL.h>
#include <map>

namespace wolfenstein {

class Render3DTexture : public IRender
{
  public:
	Render3DTexture(const RenderConfig config);
	~Render3DTexture();
	void Render(std::vector<std::shared_ptr<ICharacter>>) override;
	void SetMap(std::shared_ptr<wolfenstein::Map> map);

  private:
	void RenderPlayer(std::vector<std::shared_ptr<ICharacter>> characters);
	void ClearScreen();

	void RenderTexture(int texture_id, vector2i start, vector2i end);

	SDL_Window* window_;
	SDL_Renderer* renderer_;

	std::shared_ptr<wolfenstein::Map> map_;
	RenderConfig config_;
	std::map<int, SDL_Texture*> texture_map_;
};

}  // namespace wolfenstein

#endif	// RENDER_RENDER_3D_TEXTURE_H