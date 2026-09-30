#include "Allocators/monotonic_arena.h"
#include "Allocators/asan.h"
#include <bit>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory_resource>
#include <new>
#include <vector>

namespace karakale::memory {
namespace {

bool IsAligned(const void* pointer, std::size_t alignment) {
	return (std::bit_cast<std::uintptr_t>(pointer) & (alignment - 1)) == 0;
}

TEST(MonotonicArena, HonoursEveryAlignment) {
	MonotonicArena arena(4096);
	for (const std::size_t alignment : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u}) {
		// Knock the offset off any natural alignment
		(void)arena.allocate(1, 1);
		void* pointer = arena.allocate(24, alignment);
		EXPECT_TRUE(IsAligned(pointer, alignment)) << alignment;
	}
}

TEST(MonotonicArena, AllocationsDoNotOverlap) {
	MonotonicArena arena(1024);
	auto* first = static_cast<std::byte*>(arena.allocate(100, 8));
	auto* second = static_cast<std::byte*>(arena.allocate(100, 8));
	EXPECT_TRUE(second + 100 <= first || first + 100 <= second);
}

TEST(MonotonicArena, TracksUsageAndHighWaterMark) {
	MonotonicArena arena(1024);
	(void)arena.allocate(100, 1);
	(void)arena.allocate(50, 1);
	EXPECT_EQ(arena.Used(), 150u);
	EXPECT_EQ(arena.HighWaterMark(), 150u);

	arena.Reset();
	EXPECT_EQ(arena.Used(), 0u);
	(void)arena.allocate(10, 1);
	EXPECT_EQ(arena.HighWaterMark(), 150u);	 // the peak survives a reset
}

TEST(MonotonicArena, ResetReusesTheSameMemory) {
	MonotonicArena arena(1024);
	void* before = arena.allocate(64, 16);
	arena.Reset();
	EXPECT_EQ(arena.allocate(64, 16), before);
}

// A blown budget is reported, never silently served from the heap
TEST(MonotonicArena, ThrowsWhenFull) {
	MonotonicArena arena(128);
	(void)arena.allocate(100, 1);
	EXPECT_THROW((void)arena.allocate(64, 1), std::bad_alloc);
	EXPECT_EQ(arena.Used(), 100u);	// a failed allocation changes nothing
	EXPECT_NO_THROW((void)arena.allocate(28, 1));
}

TEST(MonotonicArena, BacksStandardContainers) {
	MonotonicArena arena(std::size_t{64} * 1024);
	std::pmr::vector<int> numbers(&arena);
	for (int i = 0; i < 1000; ++i) {
		numbers.push_back(i);
	}
	EXPECT_EQ(numbers[999], 999);
	EXPECT_GT(arena.Used(), 1000 * sizeof(int));
}

#ifdef KARAKALE_ASAN
// Under AddressSanitizer, memory released by Reset() is poisoned. The object
// is 8-byte aligned and sized, matching AddressSanitizer's 8-byte shadow
// granules; see asan.h for smaller or unaligned regions
TEST(MonotonicArenaDeathTest, UseAfterResetIsReported) {
	EXPECT_DEATH(
		{
			MonotonicArena arena(1024);
			auto* values = static_cast<std::int64_t*>(
				arena.allocate(64, alignof(std::int64_t)));
			values[3] = 1;
			arena.Reset();
			static_cast<volatile std::int64_t*>(values)[3] = 2;
		},
		"use-after-poison");
}
#endif

}  // namespace
}  // namespace karakale::memory
