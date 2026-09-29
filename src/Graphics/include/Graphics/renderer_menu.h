/**
 * @file renderer_menu.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Main menu, weapon selection, controls, settings and pause screens
 * @version 0.2
 * @date 2026-09-25
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_MENU_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_MENU_H_

#include "Graphics/renderer_interface.h"
#include "UI/ui.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wolfenstein {

struct LevelStats;

enum class MenuScreen : std::uint8_t {
	Main,
	DifficultySelect,  // a new game's difficulty
	Controls,
	Settings,
	Pause,
	Result
};

struct MenuAction
{
	enum class Type : std::uint8_t {
		None,
		StartGame,		  // at `difficulty`
		Resume,			  // leave the pause screen
		QuitToMenu,		  // abandon the current game
		Quit,			  // close the application (native only)
		SettingsChanged,  // apply Settings::Get() (e.g. volume)
		Continue,		  // go on with the saved game
	};
	Type type = Type::None;
	// Index into the difficulties the menu was given
	std::size_t difficulty = 0;
};

// A difficulty a new game offers
struct DifficultyChoice
{
	std::string label;
	std::string description;
};

class Menu
{
  public:
	// Offers the given difficulties (from easiest), which outlive the menu
	Menu(RendererContext& context,
		 std::span<const DifficultyChoice> difficulties = {});

	void Open(MenuScreen screen);
	MenuScreen GetScreen() const { return screen_; }

	void HandleEvent(const SDL_Event& event);
	// Draws the current screen without presenting, so the pause and result
	// screens can draw over the game frame, and returns the player's choice
	MenuAction Update(double delta_time);
	// Draws a level's title and name across the middle of the screen, at the
	// given opacity, over whatever is already drawn
	void DrawLevelBanner(std::string_view title, std::string_view name,
						 Uint8 alpha);
	// Offers to go on with a saved game, described on the main screen
	// ("LEVEL 3 · THE CATACOMBS · HARD"); empty offers none
	void SetSavedGame(std::string_view description) {
		saved_game_ = ui::FixedText<96>("{}", description);
		has_saved_game_ = !description.empty();
	}
	// A short message across the lower middle of the screen, over the game
	void DrawNotice(std::string_view text);
	// The level's current objective, at the top of the screen; empty draws
	// nothing
	void DrawObjective(std::string_view text);
	// The weapon slots above the ammo: those carried (a bit per slot in
	// `owned`) plain, the one in hand (`held`) lit, the rest dim
	void DrawWeaponSlots(std::size_t count, std::uint8_t owned,
						 std::size_t held);
	// The enemies killed of the level's total, below the corner map
	void DrawEnemyCounter(std::size_t kills, std::size_t enemies);
	// A level's briefing over black: its `heading`, the story wrapped to the
	// screen, and its objectives; `prompt`: whether to ask for a key to start
	void DrawBriefing(std::string_view heading, std::string_view story,
					  std::span<const std::string_view> objectives,
					  bool prompt);
	// A cleared level's results under its `heading` ("LEVEL 1 · CHECKPOINT"),
	// over black, and what was learnt there (`debrief`, optional); `prompt`:
	// whether to ask for a key to go on
	void DrawLevelStats(std::string_view heading, const LevelStats& stats,
						std::string_view debrief, bool prompt);
	// A page of intel the player took, over the view: what it is, and what
	// it says; `opacity` 1, fading to 0 as it goes
	void DrawDocument(std::string_view title, std::string_view text,
					  double opacity);
	// A page of the story over black: a small `heading` ("CHAPTER II") over
	// its `title`, its text, and which page of how many
	void DrawStoryPage(std::string_view heading, std::string_view title,
					   std::string_view text, std::size_t page,
					   std::size_t pages, bool prompt);

  private:
	// Draws `text` from (left, y), broken into lines no wider than `width`
	// between words; returns the y below its last line
	int DrawWrapped(std::string_view text, int left, int y, int width,
					ui::FontStyle style, SDL_Color colour, bool draw = true);
	MenuAction MainScreen();
	MenuAction DifficultySelectScreen();
	MenuAction ControlsScreen();
	MenuAction SettingsScreen();
	MenuAction PauseScreen();
	MenuAction ResultScreen();

	void DrawBackground();
	void DrawDimmer(Uint8 alpha);
	void DrawHint(std::string_view text);
	// Leaves Controls or Settings for the screen they were opened from
	void GoBack();

	RendererContext* context_;
	std::unique_ptr<ui::Ui> ui_;
	ui::Input input_;
	MenuScreen screen_ = MenuScreen::Main;
	MenuScreen return_screen_ = MenuScreen::Main;
	ui::FixedText<96> saved_game_{"{}", ""};
	bool has_saved_game_ = false;
	std::span<const DifficultyChoice> difficulties_;
	// The difficulty chosen for the game about to start; at first the
	// middle one (normal)
	std::size_t chosen_difficulty_ = 0;
	int background_texture_ = 0;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_MENU_H_
