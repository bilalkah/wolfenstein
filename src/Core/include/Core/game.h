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
#include "Core/scene.h"
#include "Core/world.h"
#include "GameMap/map.h"
#include "Graphics/minimap.h"
#include "Graphics/renderer_2d.h"
#include "Graphics/renderer_3d.h"
#include "Graphics/renderer_interface.h"
#include "Graphics/renderer_menu.h"
#include "Graphics/renderer_result.h"
#include "Math/vector.h"
#include "Settings/saved_game.h"
#include "TextureManager/texture_manager.h"
#include "TimeManager/time_manager.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wolfenstein {

enum class RenderType : std::uint8_t { TEXTURE, LINE };

enum class GameState : std::uint8_t { Menu, Playing, Paused, Result };

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
	// The browser main loop keeps a pointer to the game
	Game(const Game&) = delete;
	Game& operator=(const Game&) = delete;
	Game(Game&&) = delete;
	Game& operator=(Game&&) = delete;
	~Game() = default;

	void Run();
	// Runs a single frame; returns false once the game should close
	bool Tick();
	// Skips the menu and plays the given number of frames with a fixed time
	// step, then prints a JSON performance report and stops
	void StartBenchmark(int frames);
	bool IsBenchmark() const;
	// Plays a scripted session of at least 1000 frames through every screen
	// and view, a level transition, a death and a new game, then prints a
	// SOAK_RESULT line with the allocations made after startup and stops
	void StartSoak(int frames);
	// Lets P switch to the full 2D view of the level, with its enemies,
	// paths and rays: for developers, not players
	void EnableDebugView() { debug_view_ = true; }

  private:
	void Init();
	// Fresh player and level 1, keeping the window, camera and menu
	// A fresh game with the named weapon, in the campaign (or in `level`)
	// `difficulty`: index into the configuration's difficulties; scripted
	// runs always play the normal one
	void NewGame(std::string_view weapon_name, std::string_view level = {},
				 std::size_t difficulty = 0);
	// Goes on with the saved game, at the level it was saved at
	void ContinueSavedGame();
	// What every game does as it starts, new or continued
	void BeginGame();
	void SaveProgress();
	void AutoSave(double delta_time);
	void DescribeSavedGame();
	// Points every view at the world's current level
	void ShowLevel();
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

	PlayerCommand SampleCommand() const;
	void UpdateAndRender();
	void CheckGameEvent();
	void CheckGameOver();
	void AdvanceTransition(double delta_time);
	// From the results screen to the next level, or to the win screen after
	// the last
	void ContinueFromStats();
	void DrawTransition();
	// The level as the player sees it (3D, or the debug view) with the map
	// over it
	void RenderView(double alpha);
	void BenchmarkStep();
	void SoakStep();
	// Benchmark or soak: no input devices, a fixed time step
	bool IsScripted() const;

	// Declared in dependency order, destroyed in the reverse: the views,
	// which borrow the world, then the world (level, player, sound), then
	// the renderer context (textures, then the SDL renderer and SDL itself)
	std::unique_ptr<Camera2D> camera_;
	std::unique_ptr<RendererContext> renderer_context_;
	std::unique_ptr<World> world_;
	// Both views are built once; switching between them (P, debug) swaps the
	// pointer instead of building a renderer each time
	std::unique_ptr<Renderer3D> renderer_3d_;
	std::unique_ptr<Renderer2D> renderer_2d_;
	// The explored part of the level, in a corner or large (M)
	// The campaign the player can go on with, if any
	std::optional<SavedGame> saved_game_;
	double since_save_ = 0.0;
	// The difficulties a new game offers
	std::vector<DifficultyChoice> difficulty_choices_;
	std::unique_ptr<Minimap> minimap_;
	bool map_expanded_ = false;
	bool debug_view_ = false;
	IRenderer* renderer_ = nullptr;
	std::unique_ptr<Menu> menu_;
	// Built in place when a game ends, so it needs no allocation
	std::optional<RendererResult> renderer_result_;

	FrameClock clock_;
	// The simulation runs at 60 ticks per second whatever the frame rate
	FixedStep step_{1.0 / 60.0, 0.25};
	GeneralConfig config_;
	GameState state_ = GameState::Menu;
	bool running_ = true;
	RenderType render_type_ = RenderType::TEXTURE;
	double result_delay_time_ = 0.0;
	// Between levels: fading out of the cleared one, or into the next
	// Out: to black after a cleared level; Stats: its results over black,
	// until the player goes on; In: the next level from black
	enum class Fade : std::uint8_t { None, Out, Stats, In };
	Fade fade_ = Fade::None;
	double fade_time_ = 0.0;
	double cleared_time_ = 0.0;	 // since the level's last enemy died
	// The cleared level's results, taken as it fades out
	LevelStats cleared_stats_;
	// Web: set once the browser grants pointer lock, so losing it pauses
	bool had_pointer_lock_ = false;
	int benchmark_frames_ = 0;
	int soak_frames_ = 0;
	int soak_frame_ = 0;
	std::uint64_t soak_allocations_ = 0;
	std::uint64_t soak_bytes_ = 0;
	std::size_t soak_phase_ = 0;
	std::size_t soak_max_level_ = 0;
	bool soak_saw_result_ = false;
	bool soak_took_pickup_ = false;
	bool soak_opened_door_ = false;
	bool soak_saw_stats_ = false;
	int soak_first_allocation_ = -1;
	std::array<std::uint64_t, 12> soak_phase_start_{};
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_GAME_H_
