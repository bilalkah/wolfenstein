#include "Allocators/monotonic_arena.h"
#include "Allocators/asan.h"
#include <algorithm>

namespace wolfenstein::memory {

namespace {

// The block is aligned for any fundamental type; larger alignments are
// satisfied by padding inside the block
constexpr std::size_t kBlockAlignment = alignof(std::max_align_t);

}  // namespace

MonotonicArena::MonotonicArena(std::size_t capacity,
							   std::pmr::memory_resource* upstream)
	: upstream_(upstream),
	  capacity_(capacity),
	  begin_(static_cast<std::byte*>(
		  upstream_->allocate(capacity_, kBlockAlignment))),
	  end_(begin_ + capacity_),
	  current_(end_) {
	PoisonRegion(begin_, capacity_);
}

MonotonicArena::~MonotonicArena() {
	UnpoisonRegion(begin_, capacity_);
	upstream_->deallocate(begin_, capacity_, kBlockAlignment);
}

void MonotonicArena::Reset() noexcept {
	high_water_mark_ = std::max(high_water_mark_, Used());
	PoisonRegion(current_, Used());
	current_ = end_;
}

}  // namespace wolfenstein::memory
