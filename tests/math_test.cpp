#include "Math/vector.h"
#include <gtest/gtest.h>
#include <numbers>

namespace wolfenstein {
namespace {

constexpr double kPi = std::numbers::pi;

TEST(Math, SumRadianWrapsIntoMinusPiToPi) {
	EXPECT_NEAR(SumRadian(0.5, 0.25), 0.75, 1e-12);
	EXPECT_NEAR(SumRadian(kPi - 0.1, 0.2), -kPi + 0.1, 1e-12);
	EXPECT_NEAR(SumRadian(-kPi + 0.1, -0.2), kPi - 0.1, 1e-12);
}

TEST(Math, DegreeRadianRoundTrip) {
	EXPECT_NEAR(ToRadians(180.0), kPi, 1e-12);
	EXPECT_NEAR(ToDegrees(kPi / 2), 90.0, 1e-12);
	EXPECT_NEAR(ToDegrees(ToRadians(37.5)), 37.5, 1e-12);
}

TEST(Math, Distance) {
	EXPECT_DOUBLE_EQ(Distance(vector2d{0, 0}, vector2d{3, 4}), 5.0);
	EXPECT_DOUBLE_EQ(Distance(vector2i{1, 1}, vector2i{4, 5}), 5.0);
}

}  // namespace
}  // namespace wolfenstein
