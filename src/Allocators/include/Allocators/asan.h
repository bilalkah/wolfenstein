/**
 * @file asan.h
 * @brief Helpers to poison and unpoison memory for AddressSanitizer
 *
 * Custom allocators hand out memory that the sanitizer considers valid for
 * the whole block. Poisoning the parts that are not currently allocated lets
 * AddressSanitizer report accesses to them (e.g. after an arena reset) just
 * as it does for freed heap memory. Without the sanitizer these compile to
 * nothing.
 *
 * AddressSanitizer tracks memory in 8-byte granules. A poisoned region that
 * only partly covers a granule is recorded as "the first N bytes are valid",
 * so an access to it is still caught but may be reported as a buffer
 * overflow rather than use-after-poison.
 */

#ifndef ALLOCATORS_INCLUDE_ALLOCATORS_ASAN_H
#define ALLOCATORS_INCLUDE_ALLOCATORS_ASAN_H

#include <cstddef>

#if defined(__SANITIZE_ADDRESS__)
#define KARAKALE_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define KARAKALE_ASAN 1
#endif
#endif

#ifdef KARAKALE_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace karakale::memory {

inline void PoisonRegion([[maybe_unused]] const void* address,
						 [[maybe_unused]] std::size_t size) noexcept {
#ifdef KARAKALE_ASAN
	__asan_poison_memory_region(address, size);
#endif
}

inline void UnpoisonRegion([[maybe_unused]] const void* address,
						   [[maybe_unused]] std::size_t size) noexcept {
#ifdef KARAKALE_ASAN
	__asan_unpoison_memory_region(address, size);
#endif
}

}  // namespace karakale::memory

#endif	// ALLOCATORS_INCLUDE_ALLOCATORS_ASAN_H
