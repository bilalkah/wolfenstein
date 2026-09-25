#include "Allocators/object_pool.h"
#include "Allocators/asan.h"
#include "Allocators/monotonic_arena.h"
#include <bit>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace wolfenstein::memory {
namespace {

// Counts constructions and destructions; no default constructor, so the
// pool must build it in place from arguments
struct Tracked
{
	Tracked(int initial, int& counter) : value(initial), live(&counter) {
		++counter;
	}
	~Tracked() { --*live; }
	Tracked(const Tracked&) = delete;
	Tracked& operator=(const Tracked&) = delete;
	Tracked(Tracked&&) = delete;
	Tracked& operator=(Tracked&&) = delete;

	int value;
	int* live;
};

TEST(ObjectPool, CreatesAndFindsObjects) {
	int live = 0;
	ObjectPool<Tracked> pool(4);
	const auto handle = pool.Create(7, live);
	ASSERT_TRUE(handle.has_value());
	ASSERT_NE(pool.Get(*handle), nullptr);
	EXPECT_EQ(pool.Get(*handle)->value, 7);
	EXPECT_EQ(pool.Size(), 1u);
	EXPECT_EQ(live, 1);
}

// The point of generations: a handle to a destroyed object never reaches
// the object that later reuses its slot
TEST(ObjectPool, StaleHandlesAreRejected) {
	int live = 0;
	ObjectPool<Tracked> pool(1);
	const Handle<Tracked> first = *pool.Create(1, live);
	EXPECT_TRUE(pool.Destroy(first));
	EXPECT_EQ(live, 0);

	const Handle<Tracked> second = *pool.Create(2, live);
	EXPECT_EQ(second.index, first.index);  // the slot is reused
	EXPECT_NE(second, first);
	EXPECT_EQ(pool.Get(first), nullptr);
	EXPECT_FALSE(pool.Destroy(first));
	EXPECT_EQ(pool.Get(second)->value, 2);
}

TEST(ObjectPool, ReportsAFullPoolAsAValue) {
	int live = 0;
	ObjectPool<Tracked> pool(2);
	ASSERT_TRUE(pool.Create(1, live));
	ASSERT_TRUE(pool.Create(2, live));
	const auto third = pool.Create(3, live);
	ASSERT_FALSE(third.has_value());
	EXPECT_EQ(third.error(), PoolError::Full);
	EXPECT_EQ(live, 2);
}

TEST(ObjectPool, DestroysRemainingObjectsWithThePool) {
	int live = 0;
	{
		ObjectPool<Tracked> pool(8);
		for (int i = 0; i < 5; ++i) {
			ASSERT_TRUE(pool.Create(i, live));
		}
		EXPECT_EQ(live, 5);
	}
	EXPECT_EQ(live, 0);
}

TEST(ObjectPool, VisitsLiveObjectsInSlotOrder) {
	ObjectPool<std::string> pool(4);
	const auto a = *pool.Create("a");
	const auto b = *pool.Create("b");
	const auto c = *pool.Create("c");
	pool.Destroy(b);

	std::string visited;
	pool.ForEach([&](Handle<std::string> handle, std::string& value) {
		EXPECT_NE(pool.Get(handle), nullptr);
		visited += value;
	});
	EXPECT_EQ(visited, "ac");
	(void)a;
	(void)c;
}

TEST(ObjectPool, RespectsOverAlignedTypes) {
	struct alignas(64) CacheLine
	{
		int value;
	};
	ObjectPool<CacheLine> pool(3);
	for (int i = 0; i < 3; ++i) {
		const auto* object = pool.Get(*pool.Create(i));
		EXPECT_EQ(std::bit_cast<std::uintptr_t>(object) % 64, 0u);
	}
}

// All of a pool's memory can come from a level arena
TEST(ObjectPool, TakesItsStorageFromAnArena) {
	MonotonicArena arena(std::size_t{64} * 1024);
	int live = 0;
	{
		ObjectPool<Tracked> pool(100, &arena);
		const std::size_t used = arena.Used();
		EXPECT_GE(used, 100 * sizeof(Tracked));
		for (int i = 0; i < 100; ++i) {
			ASSERT_TRUE(pool.Create(i, live));
		}
		EXPECT_EQ(arena.Used(), used);	// creating objects takes no memory
	}
	EXPECT_EQ(live, 0);
}

#ifdef WOLFENSTEIN_ASAN
TEST(ObjectPoolDeathTest, UseAfterDestroyIsReported) {
	EXPECT_DEATH(
		{
			int live = 0;
			ObjectPool<Tracked> pool(2);
			const auto handle = *pool.Create(1, live);
			Tracked* object = pool.Get(handle);
			pool.Destroy(handle);
			static_cast<volatile int&>(object->value) = 2;
		},
		"use-after-poison");
}
#endif

}  // namespace
}  // namespace wolfenstein::memory
