// A campaign in progress is kept between sessions as a small text record

#include "Settings/saved_game.h"
#include "Profiler/profiler.h"
#include "Settings/storage.h"
#include <array>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace karakale {
namespace {

constexpr SavedGame kSaved{.level = 2,
						   .weapon = 1,
						   .difficulty = 2,
						   .seed = 0xDEADBEEFCAFEF00DULL,
						   .health = 72.5,
						   .weapons = 0b1011,
						   .ammo = {0, 1, 0, 2},
						   .reserve = {0, 24, 0, 16}};

TEST(SavedGame, SurvivesARoundTrip) {
	std::array<char, 256> buffer{};
	const std::size_t size = kSaved.Format(buffer);
	ASSERT_GT(size, 0u);
	EXPECT_EQ(buffer[size], '\0');
	const auto parsed = SavedGame::Parse(std::string_view(buffer.data(), size));
	ASSERT_TRUE(parsed);
	EXPECT_EQ(parsed.value_or(SavedGame{}), kSaved);
}

// A save made mid-level carries where the player stood and what is done
TEST(SavedGame, CarriesASnapshotOfTheLevel) {
	SavedGame snapshot = kSaved;
	snapshot.has_position = true;
	snapshot.x = 13.5;
	snapshot.y = 2.25;
	snapshot.theta = -1.5;
	snapshot.seconds = 87.25;
	snapshot.killed = 0b1011;
	snapshot.taken = 0b100;
	snapshot.intel = 0b10;
	snapshot.explored_cells = 20;
	snapshot.explored[0] = 0xA5;
	snapshot.explored[2] = 0x0F;
	std::array<char, 1024> buffer{};
	const std::size_t size = snapshot.Format(buffer);
	ASSERT_GT(size, 0u);
	const auto parsed = SavedGame::Parse(std::string_view(buffer.data(), size));
	ASSERT_TRUE(parsed);
	EXPECT_EQ(parsed.value_or(SavedGame{}), snapshot);
}

TEST(SavedGame, APositionIsAllThreeOrNone) {
	const std::string_view base =
		"format=4\nlevel=1\nweapon=0\ndifficulty=1\nhealth=100\n";
	EXPECT_FALSE(SavedGame::Parse(std::string(base) + "x=1.5\ny=2.5\n"));
	const auto whole =
		SavedGame::Parse(std::string(base) + "x=1.5\ny=2.5\ntheta=0\n");
	ASSERT_TRUE(whole);
	EXPECT_TRUE(whole.value_or(SavedGame{}).has_position);
	EXPECT_FALSE(SavedGame::Parse(base).value_or(SavedGame{}).has_position);
	EXPECT_FALSE(SavedGame::Parse(std::string(base) + "explored=zz\n"));
}

TEST(SavedGame, AnIncompleteOrBrokenRecordIsNoSave) {
	EXPECT_FALSE(SavedGame::Parse(""));
	EXPECT_FALSE(SavedGame::Parse("level=2\nweapon=1\n"));
	EXPECT_FALSE(SavedGame::Parse(
		"format=4\nlevel=two\nweapon=1\ndifficulty=1\nhealth=100\n"));
	// A dead player is not a game to go on with
	EXPECT_FALSE(SavedGame::Parse(
		"format=4\nlevel=2\nweapon=1\ndifficulty=1\nhealth=0\n"));
}

// A save from before games had a seed rolls its drops as seed 0
TEST(SavedGame, ASaveWithoutASeedRollsAsZero) {
	const auto parsed = SavedGame::Parse(
		"format=4\nlevel=1\nweapon=0\ndifficulty=1\nhealth=100\n");
	ASSERT_TRUE(parsed);
	EXPECT_EQ(parsed.value_or(SavedGame{.seed = 9}).seed, 0u);
}

// Weapons and pickups are saved by index: a save from before the arsenal
// or the levels changed (no format, or another one) would hand back the
// wrong ones, so it is none
TEST(SavedGame, ASaveOfAnotherFormatIsNoSave) {
	const std::string_view fields =
		"level=2\nweapon=1\ndifficulty=1\nhealth=100\n";
	EXPECT_TRUE(SavedGame::Parse(std::string("format=4\n") + fields.data()));
	EXPECT_FALSE(SavedGame::Parse(fields));
	EXPECT_FALSE(SavedGame::Parse(std::string("format=1\n") + fields.data()));
	EXPECT_FALSE(SavedGame::Parse(std::string("format=2\n") + fields.data()));
	EXPECT_FALSE(SavedGame::Parse(std::string("format=3\n") + fields.data()));
}

// A newer version's extra keys do not stop an older one reading the rest
TEST(SavedGame, UnknownKeysAreSkipped) {
	const auto parsed = SavedGame::Parse(
		"format=4\nlevel=1\nweapon=0\nsecrets=3\ndifficulty=1\nhealth=100\n"
		"armour=50\nlaser=on\n");
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

#ifdef KARAKALE_COUNTS_ALLOCATIONS
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
}  // namespace karakale
