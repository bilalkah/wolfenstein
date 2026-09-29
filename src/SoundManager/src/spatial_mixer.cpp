#include "SoundManager/spatial_mixer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace wolfenstein {

namespace {

// A sample frame: left and right, 16 bits each
constexpr std::uint32_t kFrameBytes = 4;

std::uint32_t FramesOf(const Mix_Chunk& chunk) {
	return chunk.alen / kFrameBytes;
}

}  // namespace

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

void SpatialMixer::Play(const Mix_Chunk* chunk, const vector2d& where,
						bool muffled, std::uint32_t source) {
	if (chunk != nullptr && FramesOf(*chunk) > 0) {
		Send({.kind = Command::Kind::Play,
			  .chunk = chunk,
			  .where = where,
			  .muffled = muffled,
			  .source = source});
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
			if (voice.chunk != nullptr) {
				voice.gain = Hear(ear_, theta_, voice.where, voice.muffled);
			}
		}
		return;
	}
	// The source's own voice, else a free one, else the quietest
	Voice* voice = nullptr;
	if (command.source != 0) {
		const auto own = std::ranges::find_if(voices_, [&](const Voice& v) {
			return v.chunk != nullptr && v.source == command.source;
		});
		voice = own != voices_.end() ? &*own : nullptr;
	}
	if (voice == nullptr) {
		const auto free = std::ranges::find(voices_, nullptr, &Voice::chunk);
		voice =
			free != voices_.end()
				? &*free
				: &*std::ranges::min_element(voices_, {}, [](const Voice& v) {
					  return v.gain.left + v.gain.right;
				  });
	}
	*voice = {.chunk = command.chunk,
			  .at = 0,
			  .where = command.where,
			  .muffled = command.muffled,
			  .source = command.source,
			  .gain = Hear(ear_, theta_, command.where, command.muffled)};
}

std::size_t SpatialMixer::Playing() const {
	return static_cast<std::size_t>(std::ranges::count_if(
		voices_, [](const Voice& voice) { return voice.chunk != nullptr; }));
}

void SpatialMixer::Mix(std::uint8_t* stream, int bytes) {
	// What the game asked since the last mix, in order
	std::uint32_t taken = taken_.load(std::memory_order_relaxed);
	const std::uint32_t sent = sent_.load(std::memory_order_acquire);
	for (; taken != sent; ++taken) {
		Take(commands_[taken % kCommands]);
	}
	taken_.store(taken, std::memory_order_release);

	auto* out = reinterpret_cast<std::int16_t*>(stream);
	const auto frames = static_cast<std::uint32_t>(bytes) / kFrameBytes;
	constexpr float kLowest = std::numeric_limits<std::int16_t>::min();
	constexpr float kHighest = std::numeric_limits<std::int16_t>::max();
	for (Voice& voice : voices_) {
		if (voice.chunk == nullptr) {
			continue;
		}
		const auto* in =
			reinterpret_cast<const std::int16_t*>(voice.chunk->abuf);
		const std::uint32_t count =
			std::min(frames, FramesOf(*voice.chunk) - voice.at);
		// The sound's own level, as SDL_mixer would play it, and the
		// effects' volume
		const float level = volume_.load(std::memory_order_relaxed) *
							static_cast<float>(voice.chunk->volume) /
							static_cast<float>(MIX_MAX_VOLUME);
		const float left = voice.gain.left * level;
		const float right = voice.gain.right * level;
		for (std::uint32_t i = 0; i < count; ++i) {
			const std::size_t from = 2 * static_cast<std::size_t>(voice.at + i);
			// Heard as one voice, from its place: both its channels together
			const float sample = 0.5F * (static_cast<float>(in[from]) +
										 static_cast<float>(in[from + 1]));
			std::int16_t& out_left = out[2 * static_cast<std::size_t>(i)];
			std::int16_t& out_right = out[2 * static_cast<std::size_t>(i) + 1];
			out_left = static_cast<std::int16_t>(
				std::clamp(static_cast<float>(out_left) + sample * left,
						   kLowest, kHighest));
			out_right = static_cast<std::int16_t>(
				std::clamp(static_cast<float>(out_right) + sample * right,
						   kLowest, kHighest));
		}
		voice.at += count;
		if (voice.at >= FramesOf(*voice.chunk)) {
			voice = Voice{};
		}
	}
}

void SpatialMixer::MixHook(void* mixer, std::uint8_t* stream, int bytes) {
	static_cast<SpatialMixer*>(mixer)->Mix(stream, bytes);
}

}  // namespace wolfenstein
