// The menu is drawn every frame while it is open (and over the frozen game
// when paused), so like the game frame it must not touch the heap once its
// texts are rasterised. This draws every screen through a real renderer:
// an offscreen window with the game's fonts and textures.

#include "Camera/camera.h"
#include "Graphics/renderer_interface.h"
#include "Graphics/renderer_menu.h"
#include "Profiler/profiler.h"
#include "test_services.h"
#include <gtest/gtest.h>
#include <memory>

namespace wolfenstein {
namespace {

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
TEST(Menu, FramesDoNotAllocateOnceTheirTextIsDrawn) {
	Camera2D camera(Camera2DConfig(1280, 1.0, 20.0));
	const auto context = std::make_shared<RendererContext>(
		"menu test", RenderConfig(1280, 720, 0, 32, 0, 20.0, 1.0, false),
		camera);
	Menu menu(context, testing::TestSound());
	constexpr double kFrame = 1.0 / 60.0;

	for (const MenuScreen screen :
		 {MenuScreen::Main, MenuScreen::WeaponSelect, MenuScreen::Controls,
		  MenuScreen::Settings, MenuScreen::Pause, MenuScreen::Result}) {
		menu.Open(screen);
		// The first frames rasterise the screen's texts into the cache
		for (int i = 0; i < 3; ++i) {
			(void)menu.Update(kFrame);
		}
		const auto before = AllocationStats::count;
		for (int i = 0; i < 120; ++i) {
			(void)menu.Update(kFrame);
		}
		EXPECT_EQ(AllocationStats::count - before, 0u)
			<< "screen " << static_cast<int>(screen);
	}
}
#endif

}  // namespace
}  // namespace wolfenstein
