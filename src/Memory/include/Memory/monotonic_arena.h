/**
 * @file monotonic_arena.h
 * @brief A fixed-capacity bump allocator usable as a std::pmr::memory_resource
 */

#ifndef MEMORY_INCLUDE_MEMORY_MONOTONIC_ARENA_H
#define MEMORY_INCLUDE_MEMORY_MONOTONIC_ARENA_H

#include "Memory/asan.h"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>

namespace wolfenstein::memory {

// A monotonic (bump) allocator over one block of memory, for data that lives
// and dies together, such as everything created for one level. Allocating
// moves an offset forward (with the requested alignment), deallocating does
// nothing, and Reset() releases everything at once.
//
// Compared with std::pmr::monotonic_buffer_resource:
// - it never grows: running out throws std::bad_alloc instead of silently
//   falling back to the heap, so a blown memory budget is found, not hidden;
// - Reset() rewinds the same block for the next level;
// - it reports its high-water mark, to size the budget from measurements;
// - under AddressSanitizer, memory that is not currently handed out is
//   poisoned, so an access after Reset() is reported like a use-after-free.
//
// Not thread safe.
class MonotonicArena final : public std::pmr::memory_resource
{
  public:
	// Takes the whole block from upstream once
	explicit MonotonicArena(
		std::size_t capacity,
		std::pmr::memory_resource* upstream = std::pmr::new_delete_resource());
	~MonotonicArena() override;
	// Owns its block; pmr containers hold pointers to it
	MonotonicArena(const MonotonicArena&) = delete;
	MonotonicArena& operator=(const MonotonicArena&) = delete;
	MonotonicArena(MonotonicArena&&) = delete;
	MonotonicArena& operator=(MonotonicArena&&) = delete;

	// Releases every allocation at once. Objects living in the arena are not
	// destroyed: their owners must be gone first (or be trivially
	// destructible).
	void Reset() noexcept;

	std::size_t Capacity() const noexcept { return capacity_; }
	std::size_t Used() const noexcept {
		return static_cast<std::size_t>(end_ - current_);
	}
	std::size_t Remaining() const noexcept { return capacity_ - Used(); }
	std::size_t HighWaterMark() const noexcept {
		return std::max(high_water_mark_, Used());
	}

  private:
	// Bumps downwards, from the end of the block towards its start: aligning
	// is then a single mask of the new pointer instead of a padding
	// computation. Measured (1000 x 64-byte allocations): ~2.6x faster than
	// bumping upwards and ~1.25x faster than
	// std::pmr::monotonic_buffer_resource. Defined in the header so calls
	// through a MonotonicArena (a final class) can be inlined.
	void* do_allocate(std::size_t bytes, std::size_t alignment) override {
		assert(std::has_single_bit(alignment) &&
			   "alignment must be a power of two");
		// Every allocation gets a distinct address, even an empty one
		bytes = std::max<std::size_t>(bytes, 1);

		const auto begin = std::bit_cast<std::uintptr_t>(begin_);
		auto top = std::bit_cast<std::uintptr_t>(current_);
		if (bytes > top - begin) [[unlikely]] {
			throw std::bad_alloc();
		}
		top = (top - bytes) & ~(alignment - 1);
		if (top < begin) [[unlikely]] {
			throw std::bad_alloc();
		}
		current_ = begin_ + (top - begin);
		UnpoisonRegion(current_, bytes);
		return current_;
	}
	void do_deallocate(void*, std::size_t, std::size_t) override {}
	bool do_is_equal(
		const std::pmr::memory_resource& other) const noexcept override {
		return this == &other;
	}

	std::pmr::memory_resource* upstream_;
	std::size_t capacity_;
	std::byte* begin_;
	std::byte* end_;
	std::byte* current_;  // allocations take the space below this pointer
	// Updated on Reset() rather than on every allocation; HighWaterMark()
	// also accounts for the current usage
	std::size_t high_water_mark_ = 0;
};

}  // namespace wolfenstein::memory

#endif	// MEMORY_INCLUDE_MEMORY_MONOTONIC_ARENA_H
