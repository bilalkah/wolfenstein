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
#include "Strike/weapon.h"
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
	WeaponSelect,
	Controls,
	Settings,
	Pause,
	Result
};

struct MenuAction
{
	enum class Type : std::uint8_t {
		None,
		StartGame,		  // with `weapon`
		Resume,			  // leave the pause screen
		QuitToMenu,		  // abandon the current game
		Quit,			  // close the application (native only)
		SettingsChanged,  // apply Settings::Get() (e.g. volume)
	};
	Type type = Type::None;
	// Names a weapon of the configuration the menu was given
	std::string_view weapon;
};

class Menu
{
  public:
	// Offers the given weapons, which outlive the menu (the World's config)
	Menu(RendererContext& context, SoundManager& sound,
		 std::span<const WeaponConfig> weapons);

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
	// The enemies killed of the level's total, below the corner map
	void DrawEnemyCounter(std::size_t kills, std::size_t enemies);
	// A cleared level's results under its `heading` ("LEVEL 1 · CHECKPOINT"),
	// over black; `prompt`: whether to ask for a key to go on
	void DrawLevelStats(std::string_view heading, const LevelStats& stats,
						bool prompt);

  private:
	MenuAction MainScreen();
	MenuAction WeaponSelectScreen(double delta_time);
	MenuAction ControlsScreen();
	MenuAction SettingsScreen();
	MenuAction PauseScreen();
	MenuAction ResultScreen();

	void DrawBackground();
	void DrawDimmer(Uint8 alpha);
	void DrawWeaponCard(const SDL_Rect& rect, const Weapon& weapon,
						bool focused);
	void DrawHint(std::string_view text);
	// Makes weapon `index` (-1 for none) the animated preview
	void SetPreviewedWeapon(int index);
	// Leaves Controls or Settings for the screen they were opened from
	void GoBack();

	RendererContext* context_;
	std::unique_ptr<ui::Ui> ui_;
	ui::Input input_;
	MenuScreen screen_ = MenuScreen::Main;
	MenuScreen return_screen_ = MenuScreen::Main;
	std::span<const WeaponConfig> weapon_configs_;
	std::vector<std::unique_ptr<Weapon>> weapons_;
	int previewed_weapon_ = -1;
	int background_texture_ = 0;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_MENU_H_
