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
	const std::array<std::string, 3> difficulties{"Easy", "Normal", "Hard"};
	Menu menu(context, testing::TestSound(), testing::GameData().weapons,
			  difficulties);
	// The game reads the settings at startup, before its first frame
	(void)Settings::Get();
	constexpr double kFrame = 1.0 / 60.0;

	const auto before = AllocationStats::count;
	for (const MenuScreen screen :
		 {MenuScreen::Main, MenuScreen::WeaponSelect, MenuScreen::Controls,
		  MenuScreen::Settings, MenuScreen::Pause, MenuScreen::Result}) {
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

	// Down to the difficulty, then through its choices
	SDL_Event down{};
	down.type = SDL_KEYDOWN;
	down.key.keysym.sym = SDLK_DOWN;
	for (int i = 0; i < 3; ++i) {
		menu.HandleEvent(down);
		(void)menu.Update(kFrame);
	}
	const int before_choosing = Settings::Get().difficulty;
	const auto choosing = AllocationStats::count;
	for (int i = 0; i < 6; ++i) {
		menu.HandleEvent(right);
		(void)menu.Update(kFrame);
		if (i == 0) {
			EXPECT_NE(Settings::Get().difficulty, before_choosing)
				<< "the difficulty row has focus";
		}
	}
	EXPECT_EQ(AllocationStats::count - choosing, 0u) << "choosing a difficulty";
	// Six steps through three choices come back round
	EXPECT_EQ(Settings::Get().difficulty, before_choosing);

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

}  // namespace
}  // namespace wolfenstein
