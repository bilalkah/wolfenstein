#include "SoundManager/spatial_mixer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace karakale {

StereoGain Hear(const vector2d& ear, double theta, const vector2d& source,
				bool muffled) {
	// Full up to kNear cells away, silent from kFar, falling off faster
	// near than far, as loudness does
	constexpr double kNear = 2.0;
	constexpr double kFar = 24.0;
	constexpr double kThroughWall = 0.45;
	// A sound from one side is still a little heard in the other ear
	constexpr double kWidest = 0.8;
	const vector2d to = source - ear;
	const double distance = to.Magnitude();
	const double reach =
		std::clamp((kFar - distance) / (kFar - kNear), 0.0, 1.0);
	double loud = reach * reach;
	if (muffled) {
		loud *= kThroughWall;
	}
	// +1 to the right (the way a player strafes right), -1 to the left
	const double side =
		distance < 1e-6 ? 0.0
						: kWidest * std::sin(std::atan2(to.y, to.x) - theta);
	return {.left = static_cast<float>(loud * std::min(1.0, 1.0 - side)),
			.right = static_cast<float>(loud * std::min(1.0, 1.0 + side))};
}

void SpatialMixer::Play(const SoundClip* clip, const vector2d& where,
						bool muffled, std::uint32_t source) {
	if (clip != nullptr && clip->frames > 0) {
		Send({.kind = Command::Kind::Play,
			  .clip = clip,
			  .where = where,
			  .muffled = muffled,
			  .placed = true,
			  .source = source});
	}
}

void SpatialMixer::PlayCentred(const SoundClip* clip, std::uint32_t channel) {
	if (clip != nullptr && clip->frames > 0) {
		Send({.kind = Command::Kind::Play,
			  .clip = clip,
			  .placed = false,
			  .source = channel});
	}
}

void SpatialMixer::SetListener(const vector2d& ear, double theta) {
	Send({.kind = Command::Kind::Listen, .where = ear, .theta = theta});
}

bool SpatialMixer::Send(const Command& command) {
	const std::uint32_t sent = sent_.load(std::memory_order_relaxed);
	if (sent - taken_.load(std::memory_order_acquire) == kCommands) {
		return false;
	}
	commands_[sent % kCommands] = command;
	sent_.store(sent + 1, std::memory_order_release);
	return true;
}

void SpatialMixer::Take(const Command& command) {
	if (command.kind == Command::Kind::Listen) {
		ear_ = command.where;
		theta_ = command.theta;
		for (Voice& voice : voices_) {
			if (voice.clip != nullptr && voice.placed) {
				voice.gain = Hear(ear_, theta_, voice.where, voice.muffled);
			}
		}
		return;
	}
	// The source's or channel's own voice, else a free one, else the
	// quietest
	Voice* voice = nullptr;
	if (!command.placed || command.source != 0) {
		const auto own = std::ranges::find_if(voices_, [&](const Voice& v) {
			return v.clip != nullptr && v.placed == command.placed &&
				   v.source == command.source;
		});
		voice = own != voices_.end() ? &*own : nullptr;
	}
	if (voice == nullptr) {
		const auto free = std::ranges::find(voices_, nullptr, &Voice::clip);
		voice =
			free != voices_.end()
				? &*free
				: &*std::ranges::min_element(voices_, {}, [](const Voice& v) {
					  return v.gain.left + v.gain.right;
				  });
	}
	*voice = {.clip = command.clip,
			  .at = 0,
			  .where = command.where,
			  .muffled = command.muffled,
			  .placed = command.placed,
			  .source = command.source,
			  .gain = command.placed
						  ? Hear(ear_, theta_, command.where, command.muffled)
						  : StereoGain{.left = 1.0F, .right = 1.0F}};
}

std::size_t SpatialMixer::Playing() const {
	return static_cast<std::size_t>(std::ranges::count_if(
		voices_, [](const Voice& voice) { return voice.clip != nullptr; }));
}

void SpatialMixer::Mix(float* out, int frames, int channels) {
	// What the game asked since the last mix, in order
	std::uint32_t taken = taken_.load(std::memory_order_relaxed);
	const std::uint32_t sent = sent_.load(std::memory_order_acquire);
	for (; taken != sent; ++taken) {
		Take(commands_[taken % kCommands]);
	}
	taken_.store(taken, std::memory_order_release);
	if (frames <= 0 || channels <= 0) {
		return;
	}

	const auto stride = static_cast<std::size_t>(channels);
	// Mono: left and right go into its one channel, at half each
	const std::size_t right_channel = channels > 1 ? 1 : 0;
	const float share = channels > 1 ? 1.0F : 0.5F;
	for (Voice& voice : voices_) {
		if (voice.clip == nullptr) {
			continue;
		}
		const float* in = voice.clip->samples;
		const std::uint32_t count = std::min(static_cast<std::uint32_t>(frames),
											 voice.clip->frames - voice.at);
		// The sound's own level, and the effects' volume
		const float level =
			volume_.load(std::memory_order_relaxed) * voice.clip->level * share;
		const float left = voice.gain.left * level;
		const float right = voice.gain.right * level;
		for (std::uint32_t i = 0; i < count; ++i) {
			const std::size_t from = 2 * static_cast<std::size_t>(voice.at + i);
			float* frame = out + stride * i;
			if (voice.placed) {
				// Heard as one voice, from its place: both its channels
				// together
				const float sample = 0.5F * (in[from] + in[from + 1]);
				frame[0] += sample * left;
				frame[right_channel] += sample * right;
			}
			else {
				frame[0] += in[from] * left;
				frame[right_channel] += in[from + 1] * right;
			}
		}
		voice.at += count;
		if (voice.at >= voice.clip->frames) {
			voice = Voice{};
		}
	}
}

}  // namespace karakale
