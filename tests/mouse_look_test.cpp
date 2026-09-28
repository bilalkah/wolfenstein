// The mouse moves the view as far up and down as it turns it: a mouse pixel
// slides the picture by the same number of screen pixels either way

#include "Core/game.h"
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

const GeneralConfig kView(1200, 900, 0, 20, 120, 15.0, std::numbers::pi / 3,
						  false);

// Screen pixels the picture slides: a turn across the width (width / fov
// pixels a radian), a look up as a share of the height
double Across(const MouseLook& look) {
	return look.turn * kView.screen_width / kView.fov;
}
double Down(const MouseLook& look) {
	return look.up * kView.screen_height;
}

TEST(MouseLook, UpAndDownMoveAsFarAsTurning) {
	for (const double sensitivity : {0.5, 1.0, 2.0}) {
		const MouseLook sideways = ToMouseLook(40, 0, sensitivity, kView);
		const MouseLook upwards = ToMouseLook(0, -40, sensitivity, kView);
		EXPECT_DOUBLE_EQ(Down(upwards), Across(sideways)) << sensitivity;
		EXPECT_EQ(sideways.up, 0.0);
		EXPECT_EQ(upwards.turn, 0.0);
	}
}

// The mouse pushed away (up the screen, dy < 0) looks up
TEST(MouseLook, PushingTheMouseAwayLooksUp) {
	EXPECT_GT(ToMouseLook(0, -10, 1.0, kView).up, 0.0);
	EXPECT_LT(ToMouseLook(0, 10, 1.0, kView).up, 0.0);
	EXPECT_GT(ToMouseLook(10, 0, 1.0, kView).turn, 0.0) << "right turns right";
}

}  // namespace
}  // namespace wolfenstein
