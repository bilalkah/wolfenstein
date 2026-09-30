#include "Profiler/profiler.h"
#include <algorithm>
#include <cmath>
#include <format>

namespace karakale {

namespace {

constexpr std::array<const char*,
					 static_cast<std::size_t>(ProfileSection::Count)>
	kSectionNames = {"frame",		  "update_enemies", "pathfinding",
					 "line_of_sight", "update_player",	"camera",
					 "render",		  "render_walls",	"render_objects",
					 "render_draw",	  "render_hud",		"present"};

// Nearest-rank percentile of an already sorted sample
double Percentile(const std::vector<double>& sorted, double p) {
	if (sorted.empty()) {
		return 0.0;
	}
	const auto rank = static_cast<std::size_t>(
		std::ceil(p / 100.0 * static_cast<double>(sorted.size())));
	return sorted[std::clamp<std::size_t>(rank, 1, sorted.size()) - 1];
}

std::string StatsJson(std::vector<double> values) {
	std::ranges::sort(values);
	double sum = 0.0;
	for (const double value : values) {
		sum += value;
	}
	const double mean =
		values.empty() ? 0.0 : sum / static_cast<double>(values.size());
	return std::format(
		R"({{"mean":{:.4f},"p50":{:.4f},"p95":{:.4f},"p99":{:.4f},"max":{:.4f}}})",
		mean, Percentile(values, 50), Percentile(values, 95),
		Percentile(values, 99), values.empty() ? 0.0 : values.back());
}

}  // namespace

Profiler& Profiler::GetInstance() {
	static Profiler instance;
	return instance;
}

void Profiler::Enable(std::size_t frame_capacity) {
	enabled_ = true;
	frames_.reserve(frame_capacity);
}

void Profiler::BeginFrame() {
	if (!enabled_) {
		return;
	}
	current_ = FrameSample{};
	frame_start_allocations_ = AllocationStats::count;
	frame_start_bytes_ = AllocationStats::bytes;
}

void Profiler::EndFrame() {
	if (!enabled_) {
		return;
	}
	current_.allocations = AllocationStats::count - frame_start_allocations_;
	current_.allocated_bytes = AllocationStats::bytes - frame_start_bytes_;
	frames_.push_back(current_);
}

void Profiler::Add(ProfileSection section, double milliseconds,
				   std::uint64_t allocations, std::uint64_t allocated_bytes) {
	const auto index = static_cast<std::size_t>(section);
	current_.section_ms[index] += milliseconds;
	current_.section_allocations[index] += allocations;
	current_.section_allocated_bytes[index] += allocated_bytes;
}

std::string Profiler::ReportJson(std::size_t warmup_frames) const {
	const std::size_t first = std::min(warmup_frames, frames_.size());
	const std::size_t measured = frames_.size() - first;

	// Per section statistics of one per-frame quantity
	const auto per_section = [&](auto value_of) {
		std::string json;
		for (std::size_t s = 0; s < kSectionNames.size(); ++s) {
			std::vector<double> values;
			values.reserve(measured);
			for (std::size_t f = first; f < frames_.size(); ++f) {
				values.push_back(value_of(frames_[f], s));
			}
			json += std::format(R"({}"{}":{})", s == 0 ? "" : ",",
								kSectionNames[s], StatsJson(std::move(values)));
		}
		return json;
	};
	const std::string sections =
		per_section([](const FrameSample& frame, std::size_t s) {
			return frame.section_ms[s];
		});
	const std::string section_allocations =
		per_section([](const FrameSample& frame, std::size_t s) {
			return static_cast<double>(frame.section_allocations[s]);
		});
	const std::string section_bytes =
		per_section([](const FrameSample& frame, std::size_t s) {
			return static_cast<double>(frame.section_allocated_bytes[s]);
		});

	// Allocations in the frame but outside every top-level section (events,
	// game-over checks, ...); top-level sections do not nest in each other
	std::vector<double> unattributed;
	for (std::size_t f = first; f < frames_.size(); ++f) {
		const auto& frame = frames_[f];
		std::uint64_t attributed = 0;
		for (const auto section :
			 {ProfileSection::UpdateEnemies, ProfileSection::UpdatePlayer,
			  ProfileSection::Render, ProfileSection::Present}) {
			attributed +=
				frame.section_allocations[static_cast<std::size_t>(section)];
		}
		const auto total = frame.section_allocations[static_cast<std::size_t>(
			ProfileSection::Frame)];
		unattributed.push_back(static_cast<double>(total - attributed));
	}

	std::vector<double> allocations;
	std::vector<double> allocated_bytes;
	std::string frame_ms;
	std::string frame_allocations;
	for (std::size_t f = first; f < frames_.size(); ++f) {
		const auto& frame = frames_[f];
		allocations.push_back(static_cast<double>(frame.allocations));
		allocated_bytes.push_back(static_cast<double>(frame.allocated_bytes));
		const char* separator = f == first ? "" : ",";
		frame_ms += std::format("{}{:.4f}", separator, frame.section_ms[0]);
		frame_allocations += std::format("{}{}", separator, frame.allocations);
	}

	return std::format(
		R"({{"frames":{},"warmup_frames":{},"startup_ms":{:.2f},)"
		R"("level_load":{{"ms":{:.3f},"allocations":{},"bytes":{}}},"sections_ms":{{{}}},)"
		R"("allocations_per_frame":{},"allocated_bytes_per_frame":{},)"
		R"("section_allocations":{{{}}},"section_allocated_bytes":{{{}}},)"
		R"("unattributed_allocations":{},)"
		R"("samples":{{"frame_ms":[{}],"allocations":[{}]}}}})",
		measured, first, startup_ms_, level_load_ms_, level_load_allocations_,
		level_load_bytes_, sections, StatsJson(allocations),
		StatsJson(allocated_bytes), section_allocations, section_bytes,
		StatsJson(unattributed), frame_ms, frame_allocations);
}

ScopedTimer::ScopedTimer(ProfileSection section)
	: section_(section),
	  start_(std::chrono::steady_clock::now()),
	  start_allocations_(AllocationStats::count),
	  start_bytes_(AllocationStats::bytes) {}

ScopedTimer::~ScopedTimer() {
	auto& profiler = Profiler::GetInstance();
	if (profiler.IsEnabled()) {
		const std::chrono::duration<double, std::milli> elapsed =
			std::chrono::steady_clock::now() - start_;
		profiler.Add(section_, elapsed.count(),
					 AllocationStats::count - start_allocations_,
					 AllocationStats::bytes - start_bytes_);
	}
}

}  // namespace karakale
