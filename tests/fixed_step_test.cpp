#include "Characters/character.h"
#include "TimeManager/time_manager.h"
#include <gtest/gtest.h>
#include <numbers>

namespace karakale {
namespace {

constexpr double kTick = 1.0 / 60.0;

// Faster frames than ticks: some frames simulate nothing and are drawn
// between ticks; the total still matches the time
TEST(FixedStep, FastFramesShareTicks) {
	FixedStep step(kTick, 0.25);
	int ticks = 0;
	for (int frame = 0; frame < 144; ++frame) {	 // one second at 144 fps
		ticks += step.Advance(1.0 / 144.0);
		EXPECT_GE(step.Alpha(), 0.0);
		EXPECT_LT(step.Alpha(), 1.0);
	}
	EXPECT_NEAR(ticks, 60, 1);
}

// Slower frames than ticks: each simulates several
TEST(FixedStep, SlowFramesRunSeveralTicks) {
	FixedStep step(kTick, 0.25);
	EXPECT_EQ(step.Advance(1.0 / 20.0), 3);
	EXPECT_NEAR(step.Alpha(), 0.0, 1e-9);
}

TEST(FixedStep, AStallIsNotCaughtUp) {
	FixedStep step(kTick, 0.25);
	EXPECT_EQ(step.Advance(5.0), 15);  // 0.25 s, not 5 s, of ticks
}

TEST(FixedStep, LeftoverTimeCarriesOver) {
	FixedStep step(kTick, 0.25);
	EXPECT_EQ(step.Advance(kTick * 0.75), 0);
	EXPECT_NEAR(step.Alpha(), 0.75, 1e-9);
	EXPECT_EQ(step.Advance(kTick * 0.5), 1);
	EXPECT_NEAR(step.Alpha(), 0.25, 1e-9);
	step.Reset();
	EXPECT_EQ(step.Alpha(), 0.0);
}

TEST(Interpolate, TurnsTheShortWayRound) {
	const Position2D from({0.0, 0.0}, 0.1);
	const Position2D to({2.0, 4.0}, 2.0 * std::numbers::pi - 0.1);
	const Position2D half = Interpolate(from, to, 0.5);
	EXPECT_DOUBLE_EQ(half.pose.x, 1.0);
	EXPECT_DOUBLE_EQ(half.pose.y, 2.0);
	EXPECT_NEAR(half.theta, 0.0, 1e-9);	 // through 0, not through pi
}

}  // namespace
}  // namespace karakale
