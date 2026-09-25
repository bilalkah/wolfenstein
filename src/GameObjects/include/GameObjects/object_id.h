/**
 * @file object_id.h
 * @brief Identifier of an object within its scene
 */

#ifndef GAME_OBJECTS_INCLUDE_OBJECT_ID_H
#define GAME_OBJECTS_INCLUDE_OBJECT_ID_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace wolfenstein {

// Identifies an object within its scene: its index in the scene's object
// list. Per-object data elsewhere (camera views, enemy routes) is then a
// plain array indexed by it, where string ids needed a hash map lookup (and
// a string copy) every time.
enum class ObjectId : std::uint32_t {
	None = std::numeric_limits<std::uint32_t>::max()
};

constexpr std::size_t ToIndex(ObjectId id) noexcept {
	return std::to_underlying(id);
}

}  // namespace wolfenstein

#endif	// GAME_OBJECTS_INCLUDE_OBJECT_ID_H
