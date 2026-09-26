/**
 * @file object_pool.h
 * @brief Fixed-capacity object pool addressed by generational handles
 */

#ifndef ALLOCATORS_INCLUDE_ALLOCATORS_OBJECT_POOL_H
#define ALLOCATORS_INCLUDE_ALLOCATORS_OBJECT_POOL_H

#include "Allocators/asan.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <utility>
#include <vector>

namespace wolfenstein::memory {

// Refers to an object in an ObjectPool<T>. A handle stays safe after its
// object is destroyed: the slot's generation moves on, so the pool rejects
// the stale handle instead of returning whatever reuses the slot (the
// use-after-free that raw pointers and plain indices allow). Two 32-bit
// fields, so handles are cheap to copy, compare and send over a network.
template <typename T>
struct Handle
{
	static constexpr std::uint32_t kInvalidIndex =
		std::numeric_limits<std::uint32_t>::max();

	std::uint32_t index = kInvalidIndex;
	std::uint32_t generation = 0;

	bool IsValid() const noexcept { return index != kInvalidIndex; }
	friend bool operator==(const Handle&, const Handle&) = default;
};

enum class PoolError : std::uint8_t { Full };

// Fixed-capacity storage for objects of type T. All memory is taken once, up
// front, from a std::pmr::memory_resource (a level arena, for example), so
// creating and destroying objects never touches the heap and every object
// lives in one contiguous array.
//
// Objects are built in place with std::construct_at and reached through
// std::launder, since each slot's storage is reused for new objects. Slot
// bookkeeping (generations, liveness, free list) is kept apart from the
// objects, so validating a handle does not pull the object into cache.
//
// Not thread safe. Pinned (the pool owns its storage and objects may point
// into it).
template <typename T>
class ObjectPool
{
  public:
	explicit ObjectPool(
		std::uint32_t capacity,
		std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		: resource_(resource),
		  capacity_(capacity),
		  slots_(static_cast<Slot*>(
			  resource_->allocate(sizeof(Slot) * capacity_, alignof(Slot)))),
		  generations_(capacity_, 0, resource_),
		  alive_(capacity_, 0, resource_),
		  free_list_(resource_) {
		free_list_.reserve(capacity_);
		// Hand out slot 0 first
		for (std::uint32_t i = capacity_; i-- > 0;) {
			free_list_.push_back(i);
		}
		PoisonRegion(slots_, sizeof(Slot) * capacity_);
	}

	~ObjectPool() {
		for (std::uint32_t i = 0; i < capacity_; ++i) {
			if (alive_[i]) {
				std::destroy_at(Object(i));
			}
		}
		UnpoisonRegion(slots_, sizeof(Slot) * capacity_);
		resource_->deallocate(slots_, sizeof(Slot) * capacity_, alignof(Slot));
	}

	ObjectPool(const ObjectPool&) = delete;
	ObjectPool& operator=(const ObjectPool&) = delete;
	ObjectPool(ObjectPool&&) = delete;
	ObjectPool& operator=(ObjectPool&&) = delete;

	// Builds a T in a free slot. A full pool is an expected outcome that the
	// caller handles, not an exception.
	template <typename... Args>
	[[nodiscard]] std::expected<Handle<T>, PoolError> Create(Args&&... args) {
		if (free_list_.empty()) {
			return std::unexpected(PoolError::Full);
		}
		const std::uint32_t index = free_list_.back();
		free_list_.pop_back();
		UnpoisonRegion(&slots_[index], sizeof(Slot));
		try {
			std::construct_at(
				reinterpret_cast<T*>(slots_[index].storage.data()),
				std::forward<Args>(args)...);
		}
		catch (...) {
			// Strong guarantee: the pool is unchanged if T's constructor throws
			PoisonRegion(&slots_[index], sizeof(Slot));
			free_list_.push_back(index);
			throw;
		}
		alive_[index] = 1;
		return Handle<T>{index, generations_[index]};
	}

	// Destroys the object; returns false for a stale or invalid handle
	bool Destroy(Handle<T> handle) {
		if (!IsLive(handle)) {
			return false;
		}
		std::destroy_at(Object(handle.index));
		PoisonRegion(&slots_[handle.index], sizeof(Slot));
		alive_[handle.index] = 0;
		// Outstanding handles to this slot become stale
		++generations_[handle.index];
		free_list_.push_back(handle.index);
		return true;
	}

	// The object, or nullptr if the handle is stale or invalid
	T* Get(Handle<T> handle) noexcept {
		return IsLive(handle) ? Object(handle.index) : nullptr;
	}
	const T* Get(Handle<T> handle) const noexcept {
		return IsLive(handle) ? Object(handle.index) : nullptr;
	}

	// Calls f(handle, object) for every live object, in slot order
	template <typename F>
	void ForEach(F&& f) {
		for (std::uint32_t i = 0; i < capacity_; ++i) {
			if (alive_[i]) {
				f(Handle<T>{i, generations_[i]}, *Object(i));
			}
		}
	}

	std::uint32_t Size() const noexcept {
		return capacity_ - static_cast<std::uint32_t>(free_list_.size());
	}
	std::uint32_t Capacity() const noexcept { return capacity_; }

  private:
	struct Slot
	{
		alignas(T) std::array<std::byte, sizeof(T)> storage;
	};

	bool IsLive(Handle<T> handle) const noexcept {
		return handle.index < capacity_ && alive_[handle.index] &&
			   generations_[handle.index] == handle.generation;
	}
	T* Object(std::uint32_t index) const noexcept {
		// The storage is reused for different objects over time; launder
		// makes the pointer refer to the object currently living there
		return std::launder(reinterpret_cast<T*>(slots_[index].storage.data()));
	}

	std::pmr::memory_resource* resource_;
	std::uint32_t capacity_;
	Slot* slots_;
	std::pmr::vector<std::uint32_t> generations_;
	// One byte per slot rather than std::vector<bool>, whose packed bits
	// turn every check into a shift and a mask behind a proxy reference
	std::pmr::vector<std::uint8_t> alive_;
	std::pmr::vector<std::uint32_t> free_list_;
};

}  // namespace wolfenstein::memory

#endif	// ALLOCATORS_INCLUDE_ALLOCATORS_OBJECT_POOL_H
