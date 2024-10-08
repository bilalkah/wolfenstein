/**
 * @file game.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CORE_INCLUDE_CORE_GAME_H_
#define CORE_INCLUDE_CORE_GAME_H_

#include "Camera/camera.h"
#include "Characters/player.h"
#include "CollisionManager/collision_manager.h"
#include "Core/scene.h"
#include "Graphics/renderer.h"
#include "Map/map.h"
#include "Math/vector.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"

namespace wolfenstein {

enum class RenderType { TEXTURE, LINE };

struct GeneralConfig
{
	int screen_width;
	int screen_height;
	int padding;
	int scale;
	int fps;
	double view_distance;
	double fov;
	bool fullscreen;
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
	void Update();
	void Render();
	void PrepareEnemies();
	void PrepareDynamicObjects();
	void PrepareStaticObjects();

	std::shared_ptr<Renderer> renderer_;
	std::shared_ptr<Scene> scene_;
	std::shared_ptr<Camera2D> camera_;
	std::shared_ptr<Player> player_;
	std::shared_ptr<Map> map_;

	GeneralConfig config_;
	bool is_running_;
	RenderType render_type_;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_GAME_H_
