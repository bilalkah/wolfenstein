// Replaces the global allocation functions to count heap allocations for the
// profiler. The replacement must live in the executable itself; in a static
// library the linker could drop it.

#include "Profiler/profiler.h"
#include <cstdlib>
#include <new>

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
