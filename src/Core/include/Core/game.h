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
#include "Characters/view_angles.h"
#include "Client/match_client.h"
#include "Core/scene.h"
#include "Core/story.h"
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
#include "Settings/settings.h"
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

// Joining: connecting to a multiplayer game's server, or told why not
enum class GameState : std::uint8_t { Menu, Joining, Playing, Paused, Result };

struct GeneralConfig
{
	GeneralConfig(int screen_width, int screen_height, int padding, int scale,
				  int fps, double view_distance, double base_fov,
				  bool fullscreen)
		: screen_width(screen_width),
		  screen_height(screen_height),
		  padding(padding),
		  scale(scale),
		  fps(fps),
		  view_distance(view_distance),
		  base_fov(base_fov),
		  fullscreen(fullscreen) {}

	int screen_width;
	int screen_height;
	int padding;
	int scale;
	int fps;
	double view_distance;
	double base_fov;  // see RenderConfig::base_fov
	bool fullscreen;
};

// Mouse motion as the player's view moves: a turn, in radians, and a look
// up (+) or down, as a slope. A mouse pixel moves the picture as far either
// way, however wide the view. SDL counts the motion in fractions of a pixel.
struct MouseLook
{
	double turn = 0.0;
	double up = 0.0;
};
MouseLook ToMouseLook(double dx, double dy, const Settings& settings,
					  const GeneralConfig& view);

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
	// Plays a multiplayer game: connects to the server at `url`
	// ("ws://host:port") as `name`, in place of the menu
	void Connect(const std::string& url, std::string_view name);

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
	// What every game does as it starts, new or continued: a new game opens
	// with its first level's briefing
	void BeginGame(bool brief);
	void SaveProgress();
	void AutoSave(double delta_time);
	void DescribeSavedGame();
	// Points every view at the world's current level
	void ShowLevel();
	void EnterPlaying();
	void Pause();
	// The mouse turns the view (hidden, and locked to the window) or points
	void CaptureMouse(bool captured);
	// Debug (J): another player joins, walking round in front of the view
	void JoinStandIn();
	void HandleMenuAction(const MenuAction& action);
	void ApplySettings();
	void Present();

	void MenuTick();
	// Connecting to the server, until it welcomes this player in
	void JoiningTick();
	// The server has welcomed this player: play its match
	void BeginMatch();
	// `ticks` ticks of the match: what the server said, and each tick's
	// command sent and played (the first `command`, the rest it repeated)
	void TickMatch(const PlayerCommand& command, int ticks);
	bool InMatch() const { return match_ != nullptr; }
	// A match's standing, clock, kills and (Tab, or the match over) its
	// scoreboard, over the view
	void DrawMatchHud();
	void GameTick();
	void PausedTick();
	void ResultTick();
	// Feeds the frame's events to the menu; returns false on window close
	bool PollMenuEvents();

	PlayerCommand SampleCommand();
	void UpdateAndRender();
	void CheckGameEvent();
	void CheckGameOver();
	void AdvanceTransition(double delta_time);
	// From the results screen to the next level, or to the win screen after
	// the last
	void ContinueFromStats();
	// From a level's briefing into the level
	void StartFromBriefing();
	// Tells the first `count` pages of story_ (none: nothing happens); at
	// the end of the campaign, the victory follows
	void TellStory(std::size_t count, bool ends_campaign);
	// On to the next page, or past the story
	void NextStoryPage();
	// The campaign won: its screen
	void ShowVictory();
	void DrawTransition();
	// The level as the player sees it from `eye` (3D, or the debug view)
	// with the map over it
	void RenderView(const Position2D& eye);
	// Where the view is drawn from, and how far it is tipped, `alpha` of the
	// way from the last tick to the next
	Position2D ViewPosition(double alpha) const;
	double ViewPitch(double alpha) const;
	// What undoes SDL's web backend scaling the mouse's motion (1 natively)
	double CanvasStretch() const;
	std::string_view CurrentObjective() const;
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
	// Mouse wheel steps, and the last weapon key (1 to 8, as 0 to 7; -1 for
	// none) pressed, since the last frame's command: taken from events, so a
	// tap between frames still counts
	int wheel_ = 0;
	int weapon_key_ = -1;
	bool debug_view_ = false;
	IRenderer* renderer_ = nullptr;
	std::unique_ptr<Menu> menu_;
	// Built in place when a game ends, so it needs no allocation
	std::optional<RendererResult> renderer_result_;
	// A multiplayer game's connection, and where it goes
	std::unique_ptr<MatchClient> match_;
	std::string server_url_;

	FrameClock clock_;
	// The simulation runs at 60 ticks per second whatever the frame rate
	FixedStep step_{1.0 / 60.0, 0.25};
	// The input of the frames since the last tick, for the next
	PlayerCommand pending_;
	// Where the player looks, turned by the input frame by frame and drawn
	// at once; each tick's command carries it
	ViewAngles view_;
	GeneralConfig config_;
	GameState state_ = GameState::Menu;
	bool running_ = true;
	RenderType render_type_ = RenderType::TEXTURE;
	double result_delay_time_ = 0.0;
	// Between levels: fading out of the cleared one, or into the next
	// Out: to black after a cleared level; Stats: its results over black,
	// until the player goes on; Briefing: the next level's, likewise; In:
	// the level from black
	// Between levels: the story (the opening, a chapter's card, the
	// ending) comes before a level's briefing, and after its results
	enum class Fade : std::uint8_t { None, Out, Stats, Story, Briefing, In };
	Fade fade_ = Fade::None;
	double fade_time_ = 0.0;
	double cleared_time_ = 0.0;	 // since the level's last enemy died
	// The cleared level's results, taken as it fades out
	LevelStats cleared_stats_;
	// The pages of story being told, and the one showing; told at the end
	// of the campaign, the victory follows them
	std::array<StoryPage, kStoryPages> story_{};
	std::size_t story_count_ = 0;
	std::size_t story_page_ = 0;
	bool story_ends_campaign_ = false;
	// Web: set once the browser grants pointer lock, so losing it pauses
	bool had_pointer_lock_ = false;
	// The mouse is captured for play: pointer lock on the web, relative
	// mouse mode natively
	bool captured_ = false;
	// Firing is armed once the button is up with the mouse captured, so the
	// click that captures it (or picks a menu item) is not a shot
	bool fire_armed_ = false;
	// A click since the last frame, however short
	bool clicked_ = false;
	int benchmark_frames_ = 0;
	int soak_frames_ = 0;
	int soak_frame_ = 0;
	std::uint64_t soak_allocations_ = 0;
	std::uint64_t soak_bytes_ = 0;
	std::size_t soak_phase_ = 0;
	std::size_t soak_max_level_ = 0;
	bool soak_saw_result_ = false;
	bool soak_saw_story_ = false;
	bool soak_took_pickup_ = false;
	bool soak_opened_door_ = false;
	bool soak_saw_stats_ = false;
	bool soak_saw_briefing_ = false;
	bool soak_found_secret_ = false;
	// Whether the fade in shows the level's title (not after a briefing,
	// which showed it)
	bool fade_banner_ = true;
	int soak_first_allocation_ = -1;
	std::array<std::uint64_t, 12> soak_phase_start_{};
};

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_GAME_H_
