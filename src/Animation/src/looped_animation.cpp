#include "Animation/looped_animation.h"
#include "TextureManager/texture_manager.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

namespace wolfenstein {

namespace {

// "<owner>_<clip>", built in `buffer` on the stack: as a std::string, a name
// longer than the short-string buffer (10 characters on wasm32, e.g.
// "soldier_walk") would allocate
std::string_view ClipName(std::string_view owner, std::string_view clip,
						  std::array<char, 64>& buffer) {
	const std::size_t size = owner.size() + 1 + clip.size();
	if (size > buffer.size()) {
		std::cerr << "Animation clip name too long: " << owner << '_' << clip
				  << '\n';
		std::exit(EXIT_FAILURE);
	}
	auto* out = std::ranges::copy(owner, buffer.data()).out;
	*out++ = '_';
	std::ranges::copy(clip, out);
	return {buffer.data(), size};
}

}  // namespace

std::span<const std::uint16_t> LoopedAnimation::Clip(
	const TextureManager& textures, std::string_view owner,
	std::string_view clip) {
	std::array<char, 64> buffer{};
	return textures.GetTextureCollection(ClipName(owner, clip, buffer));
}

std::span<const std::uint16_t> LoopedAnimation::FindClip(
	const TextureManager& textures, std::string_view owner,
	std::string_view clip) {
	std::array<char, 64> buffer{};
	return textures.FindTextureCollection(ClipName(owner, clip, buffer));
}

LoopedAnimation::LoopedAnimation(std::span<const std::uint16_t> frames,
								 double frame_seconds)
	: frames_(frames), frame_seconds_(frame_seconds) {}

LoopedAnimation::LoopedAnimation(const TextureManager& textures,
								 std::string_view clip, double cycle_seconds)
	: LoopedAnimation(textures.GetTextureCollection(clip), 0.0) {
	frame_seconds_ = cycle_seconds / static_cast<double>(frames_.size());
}

LoopedAnimation::LoopedAnimation(const TextureManager& textures,
								 std::string_view owner, std::string_view clip,
								 double cycle_seconds)
	: LoopedAnimation(Clip(textures, owner, clip), 0.0) {
	frame_seconds_ = cycle_seconds / static_cast<double>(frames_.size());
	// The other sides, if the art turns: all of them, each as long, or none.
	// "<clip>@<view>" is built on the stack: a level loads without
	// allocating, enemies and their animations included.
	std::array<char, 64> name{};
	if (clip.size() + 2 > name.size()) {
		std::cerr << "Animation clip name too long: " << clip << '\n';
		std::exit(EXIT_FAILURE);
	}
	auto* const at = std::ranges::copy(clip, name.data()).out;
	*at = '@';
	const std::string_view side(name.data(), clip.size() + 2);
	for (std::size_t view = 1; view < kViews; ++view) {
		at[1] = static_cast<char>('1' + view);
		const auto frames = FindClip(textures, owner, side);
		if (frames.empty() && view == 1) {
			return;	 // seen the same from every side
		}
		if (frames.size() != frames_.size()) {
			std::cerr << "Animation clip " << owner << '_' << side
					  << " is missing or not as long as the front's\n";
			std::exit(EXIT_FAILURE);
		}
		views_[view] = frames.data();
	}
	views_[0] = frames_.data();
}

void LoopedAnimation::Update(const double& delta_time) {
	counter_ += delta_time;
	if (counter_ >= frame_seconds_) {
		current_frame_ = (current_frame_ + 1) % frames_.size();
		counter_ = 0;
		if (!finished_once_ && current_frame_ == frames_.size() - 1) {
			finished_once_ = true;
		}
	}
}

void LoopedAnimation::Reset() {
	current_frame_ = 0;
	counter_ = 0;
	finished_once_ = false;
}

int LoopedAnimation::GetFrame(std::size_t view) const {
	const std::uint16_t* side = views_[view % kViews];
	return side == nullptr ? frames_[current_frame_] : side[current_frame_];
}

int LoopedAnimation::GetCurrentFrame() const {
	return frames_[current_frame_];
}

bool LoopedAnimation::IsAnimationFinishedOnce() const {
	return finished_once_;
}

}  // namespace wolfenstein
