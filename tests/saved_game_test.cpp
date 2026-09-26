// A campaign in progress is kept between sessions as a small text record

#include "Settings/saved_game.h"
#include "Profiler/profiler.h"
#include "Settings/storage.h"
#include <array>
#include <gtest/gtest.h>
#include <string_view>

namespace wolfenstein {
namespace {

constexpr SavedGame kSaved{.level = 2,
						   .weapon = 1,
						   .difficulty = 2,
						   .health = 72.5,
						   .ammo = 1,
						   .reserve = 24};

TEST(SavedGame, SurvivesARoundTrip) {
	std::array<char, 256> buffer{};
	const std::size_t size = kSaved.Format(buffer);
	ASSERT_GT(size, 0u);
	EXPECT_EQ(buffer[size], '\0');
	const auto parsed = SavedGame::Parse(std::string_view(buffer.data(), size));
	ASSERT_TRUE(parsed);
	EXPECT_EQ(parsed.value_or(SavedGame{}), kSaved);
}

TEST(SavedGame, AnIncompleteOrBrokenRecordIsNoSave) {
	EXPECT_FALSE(SavedGame::Parse(""));
	EXPECT_FALSE(SavedGame::Parse("level=2\nweapon=1\n"));
	EXPECT_FALSE(
		SavedGame::Parse("level=two\nweapon=1\ndifficulty=1\n"
						 "health=100\nammo=18\nreserve=54\n"));
	// A dead player is not a game to go on with
	EXPECT_FALSE(
		SavedGame::Parse("level=2\nweapon=1\ndifficulty=1\n"
						 "health=0\nammo=18\nreserve=54\n"));
}

// A newer version's extra keys do not stop an older one reading the rest
TEST(SavedGame, UnknownKeysAreSkipped) {
	const auto parsed = SavedGame::Parse(
		"level=1\nweapon=0\nsecrets=3\ndifficulty=1\nhealth=100\n"
		"ammo=18\nreserve=54\n");
	ASSERT_TRUE(parsed);
	EXPECT_EQ(parsed.value_or(SavedGame{}).level, 1u);
}

TEST(SavedGame, ABufferTooSmallGetsNothing) {
	std::array<char, 8> buffer{};
	EXPECT_EQ(kSaved.Format(buffer), 0u);
	EXPECT_EQ(buffer[0], '\0');
}

// The writer behind saved games and settings: numbers as the shortest text
// that reads back the same, and nothing when a line does not fit
TEST(RecordWriter, WritesKeyValueLines) {
	std::array<char, 64> buffer{};
	RecordWriter writer(buffer);
	writer.Line("volume", 0.8).Line("show_fps", 1);
	EXPECT_EQ(writer.Text(), "volume=0.8\nshow_fps=1\n");

	std::array<char, 12> small{};
	RecordWriter cut(small);
	cut.Line("volume", 0.8).Line("show_fps", 1);
	EXPECT_TRUE(cut.Text().empty());
}

#ifdef WOLFENSTEIN_COUNTS_ALLOCATIONS
// Settings are saved when the settings screen closes, which can be in the
// middle of a game
TEST(RecordWriter, WritingDoublesAllocatesNothing) {
	std::array<char, 128> buffer{};
	const auto before = AllocationStats::count;
	for (int i = 0; i < 10; ++i) {
		RecordWriter writer(buffer);
		writer.Line("mouse_sensitivity", 1.35 + i).Line("volume", 0.05 * i);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}

// Saving happens as a level starts, in play
TEST(SavedGame, FormattingAllocatesNothing) {
	std::array<char, 256> buffer{};
	const auto before = AllocationStats::count;
	for (int i = 0; i < 10; ++i) {
		(void)kSaved.Format(buffer);
	}
	EXPECT_EQ(AllocationStats::count - before, 0u);
}
#endif

}  // namespace
}  // namespace wolfenstein
