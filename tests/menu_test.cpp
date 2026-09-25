// The menu is drawn every frame while it is open (and over the frozen game
// when paused), so like the game frame it must not touch the heap once its
// texts are rasterised. This draws every screen through a real renderer:
// an offscreen window with the game's fonts and textures.

#include "Camera/camera.h"
#include "Graphics/renderer_interface.h"
#include "Graphics/renderer_menu.h"
#include "Profiler/profiler.h"
#include "Settings/settings.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <memory>

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
	Menu menu(context, testing::TestSound(), testing::GameData().weapons);
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
}
#endif

}  // namespace
}  // namespace wolfenstein
