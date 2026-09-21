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
#include "Graphics/renderer_2d.h"
#include "Graphics/renderer_3d.h"
#include "Graphics/renderer_interface.h"
#include "Graphics/renderer_menu.h"
#include "Graphics/renderer_result.h"
#include "Map/map.h"
#include "Math/vector.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <memory>
#include <string>

namespace wolfenstein {

enum class RenderType { TEXTURE, LINE };

enum class GameState { Menu, Playing, Paused, Result };

struct GeneralConfig
{
	GeneralConfig(int screen_width, int screen_height, int padding, int scale,
				  int fps, double view_distance, double fov, bool fullscreen)
		: screen_width(screen_width),
		  screen_height(screen_height),
		  padding(padding),
		  scale(scale),
		  fps(fps),
		  view_distance(view_distance),
		  fov(fov),
		  fullscreen(fullscreen) {}

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
	// Runs a single frame; returns false once the game should close
	bool Tick();
	// Skips the menu and plays the given number of frames with a fixed time
	// step, then prints a JSON performance report and stops
	void StartBenchmark(int frames);
	bool IsBenchmark() const;

  private:
	void Init();
	// Fresh player and level 1, keeping the window, camera and menu
	void NewGame(const std::string& weapon_name);
	void EnterPlaying();
	void Pause();
	void HandleMenuAction(const MenuAction& action);
	void ApplySettings();
	void Present();

	void MenuTick();
	void GameTick();
	void PausedTick();
	void ResultTick();
	// Feeds the frame's events to the menu; returns false on window close
	bool PollMenuEvents();

	void UpdateAndRender();
	void CheckGameEvent();
	void CheckGameOver();
	void BenchmarkStep();

	std::unique_ptr<IRenderer> renderer_;
	std::unique_ptr<Menu> menu_;
	std::unique_ptr<RendererResult> renderer_result_;
	std::shared_ptr<RendererContext> renderer_context_;
	std::shared_ptr<Camera2D> camera_;
	std::shared_ptr<Scene> scene_;
	std::shared_ptr<Player> player_;

	GeneralConfig config_;
	GameState state_ = GameState::Menu;
	bool running_ = true;
	RenderType render_type_ = RenderType::TEXTURE;
	double level_transition_time_ = 0.0;
	double result_delay_time_ = 0.0;
	// Web: set once the browser grants pointer lock, so losing it pauses
	bool had_pointer_lock_ = false;
	int benchmark_frames_ = 0;
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_GAME_H_
