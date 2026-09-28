// The mouse moves the view as far up and down as it turns it: a mouse pixel
// slides the picture by the same number of screen pixels either way, however
// wide the view, as far as the sensitivity says, and however many frames
// the screen draws between two ticks

#include "Core/game.h"
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

const GeneralConfig kView(1200, 900, 0, 20, 120, 15.0, std::numbers::pi / 3,
						  false);
const RenderConfig kRender(kView.screen_width, kView.screen_height, 0, 20, 120,
						   15.0, kView.base_fov, false);

Settings WithSensitivity(double sensitivity) {
	Settings settings;
	settings.mouse_sensitivity = sensitivity;
	return settings;
}

// Screen pixels the picture slides, in a view `fov` across: a turn across
// the width (width / fov pixels a radian), a look up as the slope's pixels
double Across(const MouseLook& look, double fov) {
	return look.turn * kView.screen_width / fov;
}
double Down(const MouseLook& look, double fov) {
	return look.up * PixelsPerUnit(kRender, fov);
}

TEST(MouseLook, UpAndDownMoveAsFarAsTurning) {
	for (const double sensitivity : {0.5, 1.0, 2.0}) {
		const Settings settings = WithSensitivity(sensitivity);
		const MouseLook sideways = ToMouseLook(40, 0, settings, kView);
		const MouseLook upwards = ToMouseLook(0, -40, settings, kView);
		EXPECT_DOUBLE_EQ(Down(upwards, kView.base_fov),
						 Across(sideways, kView.base_fov))
			<< sensitivity;
		EXPECT_EQ(sideways.up, 0.0);
		EXPECT_EQ(upwards.turn, 0.0);
	}
}

// A wider view shows everything smaller, up and down as across, so the
// mouse still moves the picture as far either way
TEST(MouseLook, AWiderViewKeepsThemAlike) {
	const Settings settings = WithSensitivity(1.0);
	const MouseLook sideways = ToMouseLook(40, 0, settings, kView);
	const MouseLook upwards = ToMouseLook(0, -40, settings, kView);
	for (const double degrees : {Settings::kMinFov, 80.0, Settings::kMaxFov}) {
		const double fov = degrees * std::numbers::pi / 180.0;
		EXPECT_DOUBLE_EQ(Down(upwards, fov), Across(sideways, fov)) << degrees;
	}
}

// Lower sensitivity turns less for the same mouse motion, higher more, up
// and down as across
TEST(MouseLook, SensitivityScalesIt) {
	const MouseLook slow = ToMouseLook(30, -20, WithSensitivity(0.5), kView);
	const MouseLook normal = ToMouseLook(30, -20, WithSensitivity(1.0), kView);
	const MouseLook fast = ToMouseLook(30, -20, WithSensitivity(2.0), kView);
	EXPECT_DOUBLE_EQ(slow.turn, normal.turn / 2);
	EXPECT_DOUBLE_EQ(fast.turn, normal.turn * 2);
	EXPECT_DOUBLE_EQ(slow.up, normal.up / 2);
	EXPECT_DOUBLE_EQ(fast.up, normal.up * 2);
}

// Frames come faster than ticks: none of the mouse's motion in the frames
// between two ticks is lost, however many there are
TEST(MouseLook, EveryFramesMotionReachesTheTick) {
	for (const int frames : {1, 2, 5}) {
		PlayerCommand gathered;
		for (int frame = 0; frame < frames; ++frame) {
			gathered = Gather(gathered, {.look = 0.01, .look_up = 0.02});
		}
		EXPECT_DOUBLE_EQ(gathered.look, 0.01 * frames) << frames;
		EXPECT_DOUBLE_EQ(gathered.look_up, 0.02 * frames) << frames;
	}
}

// Between ticks, movement is as it last was, a click shorter than a tick
// still fires, and a weapon chosen waits for the tick
TEST(MouseLook, EachInputGathersAsItShould) {
	const PlayerCommand clicked{.forward = 1, .fire = true, .weapon = 2};
	const PlayerCommand released{.forward = -1};
	const PlayerCommand gathered = Gather(clicked, released);
	EXPECT_EQ(gathered.forward, -1);
	EXPECT_TRUE(gathered.fire);
	EXPECT_EQ(gathered.weapon, 2);
	EXPECT_EQ(Gather(gathered, {.weapon = 1}).weapon, 1) << "the latest";
}

// The mouse pushed away (up the screen, dy < 0) looks up
TEST(MouseLook, PushingTheMouseAwayLooksUp) {
	const Settings settings = WithSensitivity(1.0);
	EXPECT_GT(ToMouseLook(0, -10, settings, kView).up, 0.0);
	EXPECT_LT(ToMouseLook(0, 10, settings, kView).up, 0.0);
	EXPECT_GT(ToMouseLook(10, 0, settings, kView).turn, 0.0)
		<< "right turns right";
}

// Inverted, pushed away it looks down; turning is as it was
TEST(MouseLook, InvertedPushingAwayLooksDown) {
	Settings inverted = WithSensitivity(1.0);
	inverted.invert_mouse_y = true;
	const Settings normal = WithSensitivity(1.0);
	EXPECT_DOUBLE_EQ(ToMouseLook(0, -10, inverted, kView).up,
					 -ToMouseLook(0, -10, normal, kView).up);
	EXPECT_LT(ToMouseLook(0, -10, inverted, kView).up, 0.0);
	EXPECT_DOUBLE_EQ(ToMouseLook(10, 0, inverted, kView).turn,
					 ToMouseLook(10, 0, normal, kView).turn);
}

}  // namespace
}  // namespace wolfenstein
