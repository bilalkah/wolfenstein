// The player's settings as they are stored: each one read back as written,
// kept in its range, and an older record (without the newer settings)
// leaving those at their defaults

#include "Settings/settings.h"
#include "Profiler/profiler.h"
#include <array>
#include <gtest/gtest.h>
#include <string_view>

namespace wolfenstein {
namespace {

Settings Changed() {
	Settings settings;
	settings.mouse_sensitivity = 1.35;
	settings.invert_mouse_y = true;
	settings.fov = 85.0;
	settings.volume = 0.6;
	settings.music_volume = 0.25;
	settings.effects_volume = 0.9;
	settings.show_fps = false;
	return settings;
}

TEST(Settings, ReadBackAsWritten) {
	const Settings written = Changed();
	std::array<char, 256> buffer{};
	const std::size_t size = written.Format(buffer);
	ASSERT_GT(size, 0u);

	Settings read;
	read.Parse(std::string_view(buffer.data(), size));
	EXPECT_EQ(read.mouse_sensitivity, written.mouse_sensitivity);
	EXPECT_EQ(read.invert_mouse_y, written.invert_mouse_y);
	EXPECT_EQ(read.fov, written.fov);
	EXPECT_EQ(read.volume, written.volume);
	EXPECT_EQ(read.music_volume, written.music_volume);
	EXPECT_EQ(read.effects_volume, written.effects_volume);
	EXPECT_EQ(read.show_fps, written.show_fps);
}

// The largest values fit the buffer Save formats into
TEST(Settings, TheLongestFit) {
	Settings settings;
	settings.mouse_sensitivity = 0.1 + 0.2;	 // 0.30000000000000004
	settings.fov = 60.000000000000014;
	settings.volume = settings.music_volume = settings.effects_volume =
		0.30000000000000004;
	std::array<char, 256> buffer{};
	EXPECT_GT(settings.Format(buffer), 0u);
}

TEST(Settings, OutOfRangeIsClamped) {
	Settings settings;
	settings.Parse(
		"fov=150\nmouse_sensitivity=0\nvolume=2\nmusic_volume=-1\n"
		"effects_volume=7\n");
	EXPECT_EQ(settings.fov, Settings::kMaxFov);
	EXPECT_EQ(settings.mouse_sensitivity, Settings::kMinMouseSensitivity);
	EXPECT_EQ(settings.volume, 1.0);
	EXPECT_EQ(settings.music_volume, 0.0);
	EXPECT_EQ(settings.effects_volume, 1.0);
	settings.Parse("fov=10\n");
	EXPECT_EQ(settings.fov, Settings::kMinFov);
}

// A record from before these settings, with a line gone bad and one this
// version does not know: what it has is read, the rest keep their defaults
TEST(Settings, AnOlderRecordKeepsTheNewDefaults) {
	Settings settings;
	settings.Parse(
		"mouse_sensitivity=2\nvolume=oops\nbrightness=3\nshow_fps=0");
	const Settings defaults;
	EXPECT_EQ(settings.mouse_sensitivity, 2.0);
	EXPECT_EQ(settings.volume, defaults.volume) << "malformed: ignored";
	EXPECT_FALSE(settings.show_fps) << "the last line, unterminated";
	EXPECT_EQ(settings.invert_mouse_y, defaults.invert_mouse_y);
	EXPECT_EQ(settings.fov, defaults.fov);
	EXPECT_EQ(settings.music_volume, defaults.music_volume);
	EXPECT_EQ(settings.effects_volume, defaults.effects_volume);
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
// Settings are saved when the settings screen closes, which can be in the
// middle of a game
TEST(Settings, FormattingAllocatesNothing) {
	const Settings settings = Changed();
	std::array<char, 256> buffer{};
	const auto before = AllocationStats::count;
	for (int i = 0; i < 10; ++i) {
		(void)settings.Format(buffer);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace wolfenstein
