// Sounds from places: heard from the side they come from, quieter the
// further off and through a wall, mixed in by the game on a fixed set of
// voices that the listener's turning pans afresh; mixing allocates nothing

#include "Profiler/profiler.h"
#include "SoundManager/spatial_mixer.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

namespace wolfenstein {
namespace {

constexpr double kPi = std::numbers::pi;
// The listener at the origin, facing +x: +y is to their right
const vector2d kEar{0.0, 0.0};

TEST(Hearing, NearAndAheadIsFullInBothEars) {
	const StereoGain gain = Hear(kEar, 0.0, {1.0, 0.0}, false);
	EXPECT_FLOAT_EQ(gain.left, 1.0F);
	EXPECT_FLOAT_EQ(gain.right, 1.0F);
}

TEST(Hearing, FurtherIsQuieterAndFarIsSilent) {
	const StereoGain near = Hear(kEar, 0.0, {4.0, 0.0}, false);
	const StereoGain further = Hear(kEar, 0.0, {12.0, 0.0}, false);
	const StereoGain far = Hear(kEar, 0.0, {30.0, 0.0}, false);
	EXPECT_GT(near.left, further.left);
	EXPECT_GT(further.left, 0.0F);
	EXPECT_FLOAT_EQ(far.left, 0.0F);
	EXPECT_FLOAT_EQ(far.right, 0.0F);
}

// From the right, louder in the right ear, and still a little in the left
TEST(Hearing, FromTheSideItComesFrom) {
	const StereoGain right = Hear(kEar, 0.0, {0.0, 3.0}, false);
	EXPECT_GT(right.right, right.left);
	EXPECT_GT(right.left, 0.0F);
	const StereoGain left = Hear(kEar, 0.0, {0.0, -3.0}, false);
	EXPECT_GT(left.left, left.right);
	// Turned to face it, it is ahead
	const StereoGain faced = Hear(kEar, kPi / 2, {0.0, 3.0}, false);
	EXPECT_FLOAT_EQ(faced.left, faced.right);
}

TEST(Hearing, AWallMuffles) {
	const StereoGain open = Hear(kEar, 0.0, {5.0, 0.0}, false);
	const StereoGain muffled = Hear(kEar, 0.0, {5.0, 0.0}, true);
	EXPECT_LT(muffled.left, open.left);
	EXPECT_GT(muffled.left, 0.0F);
}

// A short sound: every sample the same, left and right
class SpatialMixerTest : public ::testing::Test
{
  protected:
	static constexpr std::size_t kFrames = 64;

	SpatialMixerTest() {
		samples_.fill(1000);
		chunk_.abuf = reinterpret_cast<std::uint8_t*>(samples_.data());
		chunk_.alen = static_cast<std::uint32_t>(sizeof(samples_));
		chunk_.volume = MIX_MAX_VOLUME;
	}
	// A mix of `frames` sample frames into silence
	std::vector<std::int16_t> MixFrames(std::size_t frames) {
		std::vector<std::int16_t> stream(2 * frames, 0);
		mixer_.Mix(reinterpret_cast<std::uint8_t*>(stream.data()),
				   static_cast<int>(stream.size() * sizeof(std::int16_t)));
		return stream;
	}

	std::array<std::int16_t, 2 * kFrames> samples_{};
	Mix_Chunk chunk_{};
	SpatialMixer mixer_;
};

// Mixed in louder on the side it comes from; the listener turning round
// pans it to the other side
TEST_F(SpatialMixerTest, ItIsMixedInFromItsSide) {
	mixer_.SetListener(kEar, 0.0);
	mixer_.Play(&chunk_, {0.0, 3.0}, false, 0);
	const auto first = MixFrames(8);
	EXPECT_GT(first[1], first[0]) << "right louder than left";
	mixer_.SetListener(kEar, kPi);
	const auto turned = MixFrames(8);
	EXPECT_GT(turned[0], turned[1]) << "facing the other way, on the left";
}

// Played out, its voice is free again
TEST_F(SpatialMixerTest, AVoiceEndsWithItsSound) {
	mixer_.Play(&chunk_, {1.0, 0.0}, false, 0);
	MixFrames(kFrames / 2);
	EXPECT_EQ(mixer_.Playing(), 1u);
	MixFrames(kFrames);
	EXPECT_EQ(mixer_.Playing(), 0u);
}

// One voice to a source: its new sound cuts off its last
TEST_F(SpatialMixerTest, ASourceSpeaksOneAtATime) {
	mixer_.Play(&chunk_, {1.0, 0.0}, false, 7);
	mixer_.Play(&chunk_, {1.0, 0.0}, false, 7);
	mixer_.Play(&chunk_, {1.0, 0.0}, false, 0);
	MixFrames(1);
	EXPECT_EQ(mixer_.Playing(), 2u);
}

// With every voice busy, the quietest gives way to a new sound
TEST_F(SpatialMixerTest, TheQuietestGivesWay) {
	mixer_.SetListener(kEar, 0.0);
	for (std::size_t i = 0; i < SpatialMixer::kVoices; ++i) {
		// All near but one, far off
		const double off = i == 5 ? 20.0 : 1.0;
		mixer_.Play(&chunk_, {off, 0.0}, false, 0);
	}
	MixFrames(1);
	ASSERT_EQ(mixer_.Playing(), SpatialMixer::kVoices);
	const auto before = MixFrames(1);
	mixer_.Play(&chunk_, {1.0, 0.0}, false, 0);
	const auto after = MixFrames(1);
	EXPECT_GT(after[0], before[0]) << "the far one's voice now near";
	EXPECT_EQ(mixer_.Playing(), SpatialMixer::kVoices);
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
TEST_F(SpatialMixerTest, MixingAllocatesNothing) {
	std::array<std::int16_t, std::size_t{2} * 256> stream{};
	const auto before = AllocationStats::count;
	for (int round = 0; round < 100; ++round) {
		mixer_.SetListener({0.1 * round, 0.0}, 0.01 * round);
		mixer_.Play(&chunk_, {2.0, 1.0}, round % 2 == 0,
					static_cast<std::uint32_t>(round % 3));
		mixer_.Mix(reinterpret_cast<std::uint8_t*>(stream.data()),
				   static_cast<int>(sizeof(stream)));
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace wolfenstein
