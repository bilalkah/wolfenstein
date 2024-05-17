/**
 * @file game.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-05-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GAME_GAME_H
#define GAME_GAME_H

#include "character/i_character.h"
#include "character/player.h"
#include "clock/clock.h"
#include "map/map.h"
#include "render/i_render.h"
#include "render/render_2d.h"
#include "render/render_3d_simple.h"
#include <memory>
#include <vector>
#include "render/render_3d_texture.h"

namespace wolfenstein {

struct GeneralConfig
{
	int screen_width;
	int screen_height;
	int padding;
	int scale;
	int fps;
	double view_distance;
	double fov;
};

class Game
{
  public:
	Game(GeneralConfig& config);
	~Game();
	void Run();

  private:
	void Init();
	void CheckEvent();

	GeneralConfig config_;

	std::shared_ptr<Clock> clock_ptr_;
	std::shared_ptr<Map> map_ptr_;
	std::unique_ptr<Render3DTexture> render_ptr_;
	std::shared_ptr<Player> player_ptr_;
	std::vector<std::shared_ptr<ICharacter>> characters_;

	bool is_running_;
};

}  // namespace wolfenstein

#endif	// GAME_GAME_H