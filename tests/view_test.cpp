// How the view is laid out and aimed: the sky joins up all the way round,
// wherever the player looks; the view turns with the hand at once, frame by
// frame, and the tick after takes it, so a shot goes where the crosshair
// shows

#include "Characters/view_angles.h"
#include "Core/scene.h"
#include "Graphics/renderer_interface.h"
#include "test_map.h"
#include "test_services.h"
#include <cmath>
#include <gtest/gtest.h>
#include <initializer_list>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kWidth = 1200.0;  // the screen's
// The sky picture's own width at the height it is drawn
constexpr double kNatural = 256.0 * 810.0 / 128.0;

// Pixels a radian, across a view `degrees` wide
double PerRadian(double degrees) {
	return kWidth / (degrees * kPi / 180.0);
}

// A whole number of repeats a turn, each close to the picture's own width
TEST(Sky, GoesRoundAWholeNumberOfTimes) {
	for (const double degrees : {60.0, 70.0, 80.0}) {
		const SkyLayout sky = LaySky(0.0, PerRadian(degrees), kNatural);
		const double repeats = 2.0 * kPi * PerRadian(degrees) / sky.width;
		EXPECT_NEAR(repeats, std::round(repeats), 1e-9) << degrees;
		EXPECT_NEAR(sky.width / kNatural, 1.0, 0.15) << degrees;
	}
}

// No jump where the view's angle wraps round (looking along -x), nor
// anywhere else: turning a little moves it a little
TEST(Sky, JoinsUpWhereTheAngleWraps) {
	for (const double degrees : {60.0, 70.0, 80.0}) {
		const double per_radian = PerRadian(degrees);
		const SkyLayout before = LaySky(kPi - 1e-6, per_radian, kNatural);
		const SkyLayout after = LaySky(-kPi + 1e-6, per_radian, kNatural);
		const double apart = std::abs(before.offset - after.offset);
		EXPECT_LT(std::min(apart, before.width - apart), 0.01) << degrees;
	}
}

// Every offset starts the first repeat at or left of the screen's edge
TEST(Sky, StartsAtOrLeftOfTheEdge) {
	for (double theta = -kPi; theta < kPi; theta += 0.1) {
		const SkyLayout sky = LaySky(theta, PerRadian(60.0), kNatural);
		EXPECT_GE(sky.offset, 0.0) << theta;
		EXPECT_LT(sky.offset, sky.width) << theta;
	}
}

const SDL_Rect kScreen{0, 0, 1200, 900};

// A picture wholly on the screen is drawn as it is
TEST(ClipToScreen, LeavesAPictureOnTheScreenAlone) {
	SDL_Rect src{0, 0, 64, 64};
	SDL_Rect dest{100, 200, 320, 320};
	ASSERT_TRUE(ClipToScreen(src, dest, kScreen, false));
	EXPECT_EQ(src.x, 0);
	EXPECT_EQ(src.w, 64);
	EXPECT_EQ(dest.x, 100);
	EXPECT_EQ(dest.w, 320);
}

// Half off the left edge: the picture's right half, where it was and at its
// scale (10 pixels a texel)
TEST(ClipToScreen, KeepsThePartOnTheScreenAtItsScale) {
	SDL_Rect src{0, 0, 64, 64};
	SDL_Rect dest{-320, 0, 640, 640};
	ASSERT_TRUE(ClipToScreen(src, dest, kScreen, false));
	EXPECT_EQ(src.x, 32);
	EXPECT_EQ(src.w, 32);
	EXPECT_EQ(dest.x, 0);
	EXPECT_EQ(dest.w, 320);
}

// Mirrored, the screen's left shows the picture's right: half off the left
// edge, it is the picture's left half that shows
TEST(ClipToScreen, CountsAMirroredPictureFromTheOtherSide) {
	SDL_Rect src{0, 0, 64, 64};
	SDL_Rect dest{-320, 0, 640, 640};
	ASSERT_TRUE(ClipToScreen(src, dest, kScreen, true));
	EXPECT_EQ(src.x, 0);
	EXPECT_EQ(src.w, 32);
	EXPECT_EQ(dest.x, 0);
	EXPECT_EQ(dest.w, 320);
}

// A sprite right beside the eye reaches a million pixels across: what is
// drawn stays near the screen's size (uncut, a renderer drawing in software
// made an image that size, and ran out of memory)
TEST(ClipToScreen, KeepsAHugePictureNearTheScreensSize) {
	SDL_Rect src{0, 0, 64, 64};
	SDL_Rect dest{-500000, -14000, 1000000, 28800};
	ASSERT_TRUE(ClipToScreen(src, dest, kScreen, false));
	EXPECT_LE(dest.w, kScreen.w + 128);
	EXPECT_LE(dest.h, kScreen.h + 128);
	EXPECT_GE(src.w, 1);
	EXPECT_LE(src.w, 2);
}

// Wholly off the screen: nothing to draw
TEST(ClipToScreen, DropsAPictureOffTheScreen) {
	SDL_Rect src{0, 0, 64, 64};
	SDL_Rect dest{1300, 0, 100, 100};
	EXPECT_FALSE(ClipToScreen(src, dest, kScreen, false));
}

// The mouse turns the view at once, and the command carries the view, the
// motion spent
TEST(ViewAngles, TheMouseTurnsTheViewAtOnce) {
	ViewAngles view;
	view.Reset(0.5, 0.0);
	const PlayerCommand command =
		view.Apply({.look = 0.3, .look_up = 0.1}, 1.0 / 320.0);
	EXPECT_DOUBLE_EQ(view.Theta(), 0.8);
	EXPECT_DOUBLE_EQ(view.Pitch(), 0.1);
	EXPECT_TRUE(command.has_view);
	EXPECT_DOUBLE_EQ(command.view_theta, 0.8);
	EXPECT_DOUBLE_EQ(command.view_pitch, 0.1);
	EXPECT_EQ(command.look, 0.0);
	EXPECT_EQ(command.look_up, 0.0);
}

// The turning keys turn it by the frame's time, as fast whatever the frame
// rate
TEST(ViewAngles, TurningKeysTurnByTheFrame) {
	ViewAngles view;
	for (int frame = 0; frame < 320; ++frame) {
		const PlayerCommand command = view.Apply({.turn = 1}, 1.0 / 320.0);
		EXPECT_EQ(command.turn, 0);
	}
	EXPECT_NEAR(view.Theta(), Player::kKeyboardTurnSpeed, 1e-9);
}

// It looks no further up or down than the player can
TEST(ViewAngles, PitchStaysInReach) {
	ViewAngles view;
	view.Apply({.look_up = 5.0}, 0.01);
	EXPECT_DOUBLE_EQ(view.Pitch(), Player::kMaxPitch);
	view.Apply({.look_up = -9.0}, 0.01);
	EXPECT_DOUBLE_EQ(view.Pitch(), -Player::kMaxPitch);
}

// Frames between two ticks: the tick takes the latest view
TEST(ViewAngles, TheTickTakesTheLatestView) {
	const PlayerCommand first{
		.has_view = true, .view_theta = 0.1, .view_pitch = 0.05};
	const PlayerCommand second{
		.has_view = true, .view_theta = 0.2, .view_pitch = -0.05};
	EXPECT_DOUBLE_EQ(Gather(first, second).view_theta, 0.2);
	EXPECT_DOUBLE_EQ(Gather(first, second).view_pitch, -0.05);
	EXPECT_DOUBLE_EQ(Gather(first, PlayerCommand{}).view_theta, 0.1);
}

// A corridor, and the player in it
class ViewAimTest : public ::testing::Test
{
  protected:
	static constexpr SceneCapacity kCapacity{};

	ViewAimTest() {
		scene_.SetPlayer(player_);
		scene_.FinishLoading();
	}

	CharacterConfig config_{Position2D({1.5, 1.5}, 0.0), 2.0, 0.4, 0.4, 1.0};
	Map map_{testing::WriteMapFile("wolfenstein_view_test.txt",
								   {"33333", "30003", "30003", "33333"})
				 .string()};
	memory::MonotonicArena arena_{Scene::MemoryFor(map_, kCapacity)};
	Scene scene_{testing::TestTextures(), testing::TestSound(), map_, kCapacity,
				 arena_};
	Player player_{config_, testing::GameData().weapons, 0,
				   testing::TestTextures(), testing::TestSound()};
};

// The tick after, the player looks exactly where the view was drawn: its
// shots go where the crosshair showed
TEST_F(ViewAimTest, ThePlayerTakesTheViewAtTheTick) {
	ViewAngles view;
	view.Reset(player_.GetPosition().theta, player_.GetPitch());
	// Five frames of a flick on a fast screen, then the tick
	PlayerCommand pending;
	for (int frame = 0; frame < 5; ++frame) {
		pending = Gather(
			pending, view.Apply({.look = 0.04, .look_up = 0.01}, 1.0 / 320.0));
	}
	player_.SetCommand(pending);
	scene_.Update(1.0 / 60.0);
	EXPECT_DOUBLE_EQ(player_.GetPosition().theta, view.Theta());
	EXPECT_DOUBLE_EQ(player_.GetPitch(), view.Pitch());
	EXPECT_NEAR(view.Theta(), 0.2, 1e-12);
}

// A shot's kick eases back smoothly between ticks, not a tick at a time
TEST_F(ViewAimTest, TheKickEasesBetweenTicks) {
	player_.SetCommand({.fire = true});
	scene_.Update(1.0 / 60.0);
	player_.SetCommand({});
	scene_.Update(1.0 / 60.0);
	const double before = player_.GetRenderKick(0.0);
	const double after = player_.GetRenderKick(1.0);
	EXPECT_GT(before, after) << "easing back";
	EXPECT_DOUBLE_EQ(player_.GetRenderKick(0.5), (before + after) / 2);
}

}  // namespace
}  // namespace wolfenstein
