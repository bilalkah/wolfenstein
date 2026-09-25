// Microbenchmarks for the memory module against the standard alternatives.
// Build with the native-release preset and run:
//   ./scripts/dev.sh ./build/native-release/bin/memory_benchmark

#include "Memory/monotonic_arena.h"
#include "Memory/object_pool.h"
#include <array>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using wolfenstein::memory::Handle;
using wolfenstein::memory::MonotonicArena;
using wolfenstein::memory::ObjectPool;

// About the size of a small game object's hot data
struct Entity
{
	std::array<double, 8> data{};
};
static_assert(sizeof(Entity) == 64);

constexpr std::size_t kObjects = 1000;

// ---- Level lifetime: many allocations released together ------------------

void BM_LevelNewDelete(benchmark::State& state) {
	std::vector<Entity*> objects(kObjects);
	for (auto _ : state) {
		for (auto& object : objects) {
			object = new Entity;
		}
		benchmark::DoNotOptimize(objects.data());
		for (auto* object : objects) {
			delete object;
		}
	}
	state.SetItemsProcessed(state.iterations() * kObjects);
}
BENCHMARK(BM_LevelNewDelete);

void BM_LevelMonotonicArena(benchmark::State& state) {
	MonotonicArena arena(kObjects * sizeof(Entity) * 2);
	std::vector<Entity*> objects(kObjects);
	for (auto _ : state) {
		for (auto& object : objects) {
			object = static_cast<Entity*>(
				arena.allocate(sizeof(Entity), alignof(Entity)));
		}
		benchmark::DoNotOptimize(objects.data());
		arena.Reset();
	}
	state.SetItemsProcessed(state.iterations() * kObjects);
}
BENCHMARK(BM_LevelMonotonicArena);

void BM_LevelStdMonotonicBuffer(benchmark::State& state) {
	std::vector<std::byte> buffer(kObjects * sizeof(Entity) * 2);
	std::vector<Entity*> objects(kObjects);
	for (auto _ : state) {
		std::pmr::monotonic_buffer_resource resource(
			buffer.data(), buffer.size(), std::pmr::null_memory_resource());
		for (auto& object : objects) {
			object = static_cast<Entity*>(
				resource.allocate(sizeof(Entity), alignof(Entity)));
		}
		benchmark::DoNotOptimize(objects.data());
	}
	state.SetItemsProcessed(state.iterations() * kObjects);
}
BENCHMARK(BM_LevelStdMonotonicBuffer);

// ---- Entity lifetime: create and destroy one object ------------------------

void BM_ChurnMakeUnique(benchmark::State& state) {
	for (auto _ : state) {
		auto entity = std::make_unique<Entity>();
		benchmark::DoNotOptimize(entity.get());
	}
}
BENCHMARK(BM_ChurnMakeUnique);

void BM_ChurnMakeShared(benchmark::State& state) {
	for (auto _ : state) {
		auto entity = std::make_shared<Entity>();
		benchmark::DoNotOptimize(entity.get());
	}
}
BENCHMARK(BM_ChurnMakeShared);

void BM_ChurnObjectPool(benchmark::State& state) {
	ObjectPool<Entity> pool(64);
	for (auto _ : state) {
		const auto handle = pool.Create();
		benchmark::DoNotOptimize(pool.Get(*handle));
		pool.Destroy(*handle);
	}
}
BENCHMARK(BM_ChurnObjectPool);

// ---- Lookup: string ids (as the engine does today) vs handles -------------

void BM_LookupByStringId(benchmark::State& state) {
	std::unordered_map<std::string, std::unique_ptr<Entity>> entities;
	std::vector<std::string> ids;
	for (std::size_t i = 0; i < kObjects; ++i) {
		ids.push_back(std::to_string(i));
		entities.emplace(ids.back(), std::make_unique<Entity>());
	}
	std::size_t i = 0;
	for (auto _ : state) {
		Entity* entity = entities.find(ids[i])->second.get();
		benchmark::DoNotOptimize(entity);
		i = (i + 1) % kObjects;
	}
}
BENCHMARK(BM_LookupByStringId);

void BM_LookupByHandle(benchmark::State& state) {
	ObjectPool<Entity> pool(kObjects);
	std::vector<Handle<Entity>> handles;
	for (std::size_t i = 0; i < kObjects; ++i) {
		handles.push_back(*pool.Create());
	}
	std::size_t i = 0;
	for (auto _ : state) {
		Entity* entity = pool.Get(handles[i]);
		benchmark::DoNotOptimize(entity);
		i = (i + 1) % kObjects;
	}
}
BENCHMARK(BM_LookupByHandle);

}  // namespace

BENCHMARK_MAIN();
