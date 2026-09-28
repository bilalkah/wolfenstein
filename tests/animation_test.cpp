#include "Animation/looped_animation.h"
#include "Animation/triggered_single_animation.h"
#include "TextureManager/texture_manager.h"
#include <gtest/gtest.h>
#include <string>

namespace wolfenstein {
namespace {

TEST(FadeAnimation, InterpolatesTheAlpha) {
	TriggeredSingleAnimation fade(1.0, 128, 0);	 // fades out in 1 s
	EXPECT_EQ(fade.GetAlpha(), 128);
	fade.Update(0.5);
	EXPECT_EQ(fade.GetAlpha(), 64);
	EXPECT_FALSE(fade.IsAnimationFinishedOnce());
}

// The fade used to finish only when the alpha landed exactly on its end
// value: a long frame stepped past it, and the alpha went on falling below
// 0, which as a Uint8 wrapped round to fully opaque
TEST(FadeAnimation, ALongFrameStopsAtTheEndValue) {
	TriggeredSingleAnimation fade(1.0, 128, 0);
	fade.Update(0.9);
	fade.Update(0.5);  // well past the end of the fade
	EXPECT_EQ(fade.GetAlpha(), 0);
	EXPECT_TRUE(fade.IsAnimationFinishedOnce());

	fade.Reset();
	EXPECT_EQ(fade.GetAlpha(), 128);
}

TEST(FadeAnimation, FadesIn) {
	TriggeredSingleAnimation fade(0.2, 0, 255);	 // 5 s, as the end screens
	fade.Update(10.0);
	EXPECT_EQ(fade.GetAlpha(), 255);
}

// A clip seen from 8 sides plays each side's frames in step: frame i of
// view v is the side's own. A clip that looks the same from everywhere
// gives its one frame whichever side asks.
TEST(LoopedAnimation, EachSidePlaysItsOwnFrames) {
	TextureManager textures;
	textures.DefineCollection("thing_walk", 0, 2);
	for (int view = 2; view <= 8; ++view) {
		const auto first = static_cast<std::uint16_t>(10 * view);
		textures.DefineCollection("thing_walk@" + std::to_string(view), first,
								  static_cast<std::uint16_t>(first + 2));
	}
	textures.DefineCollection("thing_death", 100, 102);

	LoopedAnimation walk(textures, "thing", "walk", 1.0);
	EXPECT_EQ(walk.GetFrame(0), 0) << "the front";
	EXPECT_EQ(walk.GetFrame(4), 50) << "the back, Doom's rotation 5";
	EXPECT_EQ(walk.GetFrame(7), 80);
	walk.Update(0.6);  // past the first of two frames in a second
	EXPECT_EQ(walk.GetFrame(0), 1);
	EXPECT_EQ(walk.GetFrame(4), 51);
	EXPECT_EQ(walk.GetCurrentFrame(), 1) << "the front's";

	LoopedAnimation death(textures, "thing", "death", 1.0);
	for (std::size_t view = 0; view < LoopedAnimation::kViews; ++view) {
		EXPECT_EQ(death.GetFrame(view), 100) << view;
	}
}

}  // namespace
}  // namespace wolfenstein
