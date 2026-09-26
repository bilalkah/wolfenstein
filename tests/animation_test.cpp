#include "Animation/triggered_single_animation.h"
#include <gtest/gtest.h>

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

}  // namespace
}  // namespace wolfenstein
