/**
 * @file profiler.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief Per-frame section timings and allocation counts for benchmarking
 * @version 0.1
 * @date 2026-09-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef PROFILER_INCLUDE_PROFILER_PROFILER_H
#define PROFILER_INCLUDE_PROFILER_PROFILER_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wolfenstein {

// Sections may nest (e.g. Pathfinding runs inside UpdateEnemies), so their
// times are not meant to add up to Frame
enum class ProfileSection : std::uint8_t {
	Frame,
	UpdateEnemies,
	Pathfinding,
	LineOfSight,
	UpdatePlayer,
	Camera,
	Render,
	RenderWalls,
	RenderObjects,
	RenderDraw,
	RenderHud,
	Present,
	Count
};

// Heap allocation totals, maintained by the global operator new replacement
// linked into the executable (app/allocation_counter.cpp)
struct AllocationStats
{
	static inline std::uint64_t count = 0;
	static inline std::uint64_t bytes = 0;
};

class Profiler
{
  public:
	static Profiler& GetInstance();
	Profiler(const Profiler&) = delete;
	Profiler& operator=(const Profiler&) = delete;

	// Recording is off until Enable is called, so the timers cost two clock
	// reads and a branch during normal play
	void Enable(std::size_t frame_capacity);
	bool IsEnabled() const { return enabled_; }

	void BeginFrame();
	void EndFrame();
	void Add(ProfileSection section, double milliseconds);
	std::size_t GetFrameCount() const { return frames_.size(); }

	void AddStartupTime(double milliseconds) { startup_ms_ += milliseconds; }

	// Summary statistics per section, skipping the first warmup_frames, plus
	// the raw per-frame samples
	std::string ReportJson(std::size_t warmup_frames) const;

  private:
	Profiler() = default;

	struct FrameSample
	{
		std::array<double, static_cast<std::size_t>(ProfileSection::Count)>
			section_ms{};
		std::uint64_t allocations = 0;
		std::uint64_t allocated_bytes = 0;
	};

	bool enabled_ = false;
	double startup_ms_ = 0.0;
	FrameSample current_;
	std::uint64_t frame_start_allocations_ = 0;
	std::uint64_t frame_start_bytes_ = 0;
	std::vector<FrameSample> frames_;
};

class ScopedTimer
{
  public:
	explicit ScopedTimer(ProfileSection section);
	~ScopedTimer();
	ScopedTimer(const ScopedTimer&) = delete;
	ScopedTimer& operator=(const ScopedTimer&) = delete;

  private:
	ProfileSection section_;
	std::chrono::steady_clock::time_point start_;
};

}  // namespace wolfenstein

#endif	// PROFILER_INCLUDE_PROFILER_PROFILER_H
