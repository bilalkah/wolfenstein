// The campaign's story, told between its levels: the opening before a new
// game's first level, a chapter's card before the level that opens it, and
// the ending after the last

#include "Core/story.h"
#include <array>
#include <gtest/gtest.h>

namespace karakale {
namespace {

Campaign TwoChapters() {
	return {.opening = {{.title = "THE NUMBERS", .text = "For three weeks."},
						{.title = "YOU", .text = "You go in alone."}},
			.chapters = {{.title = "CHAPTER I",
						  .name = "THE VALLEY",
						  .text = "The road climbs.",
						  .first_level = 0,
						  .level_count = 2},
						 {.title = "CHAPTER II",
						  .name = "THE WORKS",
						  .text = "Under the valley.",
						  .first_level = 2,
						  .level_count = 1}},
			.ending = {{.title = "THE GATE", .text = "It goes dark."}}};
}

// A new game: the opening, then the first chapter's card
TEST(Story, ANewGameOpensWithTheStoryThenTheChapter) {
	const Campaign campaign = TwoChapters();
	std::array<StoryPage, kStoryPages> pages{};
	ASSERT_EQ(StoryBefore(campaign, 0, true, pages), 3u);
	EXPECT_EQ(pages[0].title, "THE NUMBERS");
	EXPECT_EQ(pages[1].text, "You go in alone.");
	EXPECT_EQ(pages[2].heading, "CHAPTER I");
	EXPECT_EQ(pages[2].title, "THE VALLEY");
}

// Going on from a saved game, or into a chapter's next level, no opening
TEST(Story, OnlyAChaptersFirstLevelHasACard) {
	const Campaign campaign = TwoChapters();
	std::array<StoryPage, kStoryPages> pages{};
	EXPECT_EQ(StoryBefore(campaign, 0, false, pages), 1u);
	EXPECT_EQ(StoryBefore(campaign, 1, false, pages), 0u);
	ASSERT_EQ(StoryBefore(campaign, 2, false, pages), 1u);
	EXPECT_EQ(pages[0].title, "THE WORKS");
}

TEST(Story, TheLastLevelEndsIt) {
	const Campaign campaign = TwoChapters();
	std::array<StoryPage, kStoryPages> pages{};
	ASSERT_EQ(StoryAtTheEnd(campaign, pages), 1u);
	EXPECT_EQ(pages[0].title, "THE GATE");
}

// No more pages than there is room for
TEST(Story, ItKeepsToItsPages) {
	Campaign campaign = TwoChapters();
	campaign.opening.resize(kStoryPages + 3, {.title = "", .text = "More."});
	std::array<StoryPage, kStoryPages> pages{};
	EXPECT_EQ(StoryBefore(campaign, 0, true, pages), kStoryPages);
}

}  // namespace
}  // namespace karakale
