/**
 * @file story.h
 * @brief The campaign's story, told between its levels
 */

#ifndef CORE_INCLUDE_CORE_STORY_H_
#define CORE_INCLUDE_CORE_STORY_H_

#include "Core/level_data.h"
#include <cstddef>
#include <span>
#include <string_view>

namespace wolfenstein {

// A page of story as drawn between levels: a small heading over a title,
// and its text; views into the game's config, which outlives them
struct StoryPage
{
	std::string_view heading;
	std::string_view title;
	std::string_view text;
};

// The most pages told at once (the opening and a chapter's card)
inline constexpr std::size_t kStoryPages = 8;

// The pages told before campaign level `level`: on a new game, before the
// first, the opening; then the card of the chapter the level opens, if it
// opens one. Writes them into `pages` and returns how many.
inline std::size_t StoryBefore(const Campaign& campaign, std::size_t level,
							   bool new_game, std::span<StoryPage> pages) {
	std::size_t count = 0;
	const auto add = [&](StoryPage page) {
		if (count < pages.size()) {
			pages[count++] = page;
		}
	};
	if (new_game && level == 0) {
		for (const StoryText& page : campaign.opening) {
			add({.heading = {}, .title = page.title, .text = page.text});
		}
	}
	if (const Chapter* chapter = campaign.ChapterOpenedBy(level)) {
		add({.heading = chapter->title,
			 .title = chapter->name,
			 .text = chapter->text});
	}
	return count;
}

// The pages told once the campaign's last level is cleared
inline std::size_t StoryAtTheEnd(const Campaign& campaign,
								 std::span<StoryPage> pages) {
	std::size_t count = 0;
	for (const StoryText& page : campaign.ending) {
		if (count < pages.size()) {
			pages[count++] = {
				.heading = {}, .title = page.title, .text = page.text};
		}
	}
	return count;
}

}  // namespace wolfenstein

#endif	// CORE_INCLUDE_CORE_STORY_H_
