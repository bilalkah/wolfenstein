// Counts heap allocations for the profiler. The replacement must live in the
// executable itself; in a static library the linker could drop it.
//
// On the web, the shipped target, this replaces malloc itself rather than
// operator new: SDL, SDL_mixer, SDL_ttf and the WebGL renderer are C code
// that never goes through operator new, so counting only C++ allocations
// would miss everything they do each frame. Only the main thread is counted:
// the per-frame counters are not atomic.
//
// Native builds count operator new only, i.e. our own code. Headless native
// runs (CI, the dev container) draw through SDL's software renderer, which
// allocates inside every SDL_RenderCopy; counting that would measure the
// test setup, not the game.

#include "Profiler/profiler.h"
#include <cerrno>
#include <cstddef>

#if defined(__EMSCRIPTEN__)
#include <emscripten/heap.h>
#include <emscripten/threading.h>

namespace {
void* RealMalloc(std::size_t size) {
	return emscripten_builtin_malloc(size);
}
void* RealCalloc(std::size_t count, std::size_t size) {
	return emscripten_builtin_calloc(count, size);
}
void* RealRealloc(void* ptr, std::size_t size) {
	return emscripten_builtin_realloc(ptr, size);
}
void* RealMemalign(std::size_t alignment, std::size_t size) {
	return emscripten_builtin_memalign(alignment, size);
}
void RealFree(void* ptr) {
	emscripten_builtin_free(ptr);
}
bool OnMainThread() {
	return emscripten_is_main_runtime_thread();
}
}  // namespace
#define WOLFENSTEIN_COUNT_MALLOC 1

#endif

#if defined(WOLFENSTEIN_COUNT_MALLOC)

namespace {
void Count(std::size_t size) {
	if (OnMainThread()) {
		wolfenstein::AllocationStats::count++;
		wolfenstein::AllocationStats::bytes += size;
	}
}
}  // namespace

// operator new keeps its default definition, which calls malloc
extern "C" {

void* malloc(std::size_t size) {
	Count(size);
	return RealMalloc(size);
}

void* calloc(std::size_t count, std::size_t size) {
	Count(count * size);
	return RealCalloc(count, size);
}

// A realloc may move the block, so it counts as an allocation
void* realloc(void* ptr, std::size_t size) {
	Count(size);
	return RealRealloc(ptr, size);
}

void* memalign(std::size_t alignment, std::size_t size) {
	Count(size);
	return RealMemalign(alignment, size);
}

void* aligned_alloc(std::size_t alignment, std::size_t size) {
	Count(size);
	return RealMemalign(alignment, size);
}

int posix_memalign(void** out, std::size_t alignment, std::size_t size) {
	Count(size);
	void* ptr = RealMemalign(alignment, size);
	if (ptr == nullptr) {
		return ENOMEM;
	}
	*out = ptr;
	return 0;
}

void free(void* ptr) {
	RealFree(ptr);
}

}  // extern "C"

#else
#include <cstdlib>
#include <new>

// Replacing the global allocation functions means calling malloc and free
// directly: this file is the allocator, so nothing here can use RAII
// NOLINTBEGIN(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
void* operator new(std::size_t size) {
	wolfenstein::AllocationStats::count++;
	wolfenstein::AllocationStats::bytes += size;
	if (void* ptr = std::malloc(size == 0 ? 1 : size)) {
		return ptr;
	}
	throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
	return ::operator new(size);
}

void operator delete(void* ptr) noexcept {
	std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
	std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
	std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
	std::free(ptr);
}
// NOLINTEND(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
#endif
