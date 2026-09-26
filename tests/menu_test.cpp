// The menu is drawn every frame while it is open (and over the frozen game
// when paused), so like the game frame it must not touch the heap once its
// texts are rasterised. This draws every screen through a real renderer:
// an offscreen window with the game's fonts and textures.

#include "Camera/camera.h"
#include "Core/scene.h"
#include "Graphics/renderer_interface.h"
#include "Graphics/renderer_menu.h"
#include "Profiler/profiler.h"
#include "Settings/settings.h"
#include "test_services.h"
#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace wolfenstein {
namespace {

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
// Every text is drawn from glyphs rasterised when the menu is built, so no
// frame allocates: not the first frame of a screen, and not a frame whose
// text is new (a slider value that just changed)
TEST(Menu, NoFrameAllocates) {
	Camera2D camera(Camera2DConfig(1280, 1.0, 20.0));
	RendererContext context("menu test",
							RenderConfig(1280, 720, 0, 32, 0, 20.0, 1.0, false),
							camera);
	const std::array<DifficultyChoice, 3> difficulties{
		{{"Easy", "Softer."}, {"Normal", "As meant."}, {"Hard", "Harder."}}};
	Menu menu(context, testing::TestSound(), testing::GameData().weapons,
			  difficulties);
	// The game reads the settings at startup, before its first frame
	(void)Settings::Get();
	// A saved game adds CONTINUE to the main screen
	menu.SetSavedGame("LEVEL 3 · THE CATACOMBS · HARD");
	constexpr double kFrame = 1.0 / 60.0;

	const auto before = AllocationStats::count;
	for (const MenuScreen screen :
		 {MenuScreen::Main, MenuScreen::DifficultySelect,
		  MenuScreen::WeaponSelect, MenuScreen::Controls, MenuScreen::Settings,
		  MenuScreen::Pause, MenuScreen::Result}) {
		menu.Open(screen);
		for (int i = 0; i < 60; ++i) {
			(void)menu.Update(kFrame);
		}
	}
	EXPECT_EQ(AllocationStats::count - before, 0u) << "opening screens";

	// Move the settings sliders, which redraws their values as new text
	menu.Open(MenuScreen::Settings);
	SDL_Event right{};
	right.type = SDL_KEYDOWN;
	right.key.keysym.sym = SDLK_RIGHT;
	const auto sliding = AllocationStats::count;
	for (int i = 0; i < 40; ++i) {
		menu.HandleEvent(right);
		(void)menu.Update(kFrame);
	}
	EXPECT_EQ(AllocationStats::count - sliding, 0u) << "moving a slider";

	// Over the game: the enemy counter, and a cleared level's results
	const LevelStats stats{.kills = 7,
						   .enemies = 10,
						   .pickups_taken = 3,
						   .pickups = 8,
						   .explored_percent = 64,
						   .seconds = 134.5};
	const auto hud = AllocationStats::count;
	for (int i = 0; i < 60; ++i) {
		menu.DrawEnemyCounter(static_cast<std::size_t>(i % 11), 10);
		menu.DrawLevelStats("LEVEL 1 · CHECKPOINT", stats, i % 2 == 0);
	}
	EXPECT_EQ(AllocationStats::count - hud, 0u) << "the counter and results";
}
#endif

// A new game asks for the difficulty once, then the weapon; the game starts
// with both
TEST(Menu, ANewGameChoosesADifficultyThenAWeapon) {
	Camera2D camera(Camera2DConfig(1280, 1.0, 20.0));
	RendererContext context("menu test",
							RenderConfig(1280, 720, 0, 32, 0, 20.0, 1.0, false),
							camera);
	const std::array<DifficultyChoice, 3> difficulties{
		{{"Easy", "Softer."}, {"Normal", "As meant."}, {"Hard", "Harder."}}};
	Menu menu(context, testing::TestSound(), testing::GameData().weapons,
			  difficulties);
	const auto press = [&](SDL_Keycode key) {
		SDL_Event event{};
		event.type = SDL_KEYDOWN;
		event.key.keysym.sym = key;
		menu.HandleEvent(event);
		return menu.Update(1.0 / 60.0);
	};

	// Opened between frames, as a click on NEW GAME would; the screen is
	// laid out once before the keys can move through it
	menu.Open(MenuScreen::DifficultySelect);
	(void)menu.Update(1.0 / 60.0);
	(void)menu.Update(1.0 / 60.0);
	(void)press(SDLK_DOWN);	 // from Normal, where it starts, to Hard
	EXPECT_EQ(press(SDLK_RETURN).type, MenuAction::Type::None);
	(void)menu.Update(1.0 / 60.0);	// the weapon screen
	const MenuAction start = press(SDLK_RETURN);
	ASSERT_EQ(start.type, MenuAction::Type::StartGame);
	EXPECT_EQ(start.difficulty, 2u);
	EXPECT_EQ(start.weapon, testing::GameData().weapons.front().weapon_name);

	// Back from the weapons goes back to the difficulty
	(void)press(SDLK_ESCAPE);
	const MenuAction again = press(SDLK_RETURN);
	(void)menu.Update(1.0 / 60.0);
	const MenuAction restart = press(SDLK_RETURN);
	EXPECT_EQ(again.type, MenuAction::Type::None);
	EXPECT_EQ(restart.difficulty, 2u) << "it remembers the choice";
}

}  // namespace
}  // namespace wolfenstein
