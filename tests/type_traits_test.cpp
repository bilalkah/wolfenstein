// Compile-time guarantees about copy and move behaviour. A declared
// destructor or a hand-written copy silently suppresses the implicit moves,
// and a copyable resource owner double-frees: these asserts turn either
// regression into a build error.

#include "Camera/ray.h"
#include "Characters/character.h"
#include "Characters/enemy.h"
#include "Core/game.h"
#include "GameObjects/dynamic_object.h"
#include "GameObjects/static_object.h"
#include "Graphics/renderer_interface.h"
#include "Strike/weapon.h"
#include "UI/ui.h"
#include <gtest/gtest.h>
#include <type_traits>

namespace karakale {
namespace {

template <typename T>
constexpr bool kPinned =
	!std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T> &&
	!std::is_move_constructible_v<T> && !std::is_move_assignable_v<T>;

// std::vector relocates elements by move only if the move cannot throw;
// otherwise every reallocation copies them
static_assert(std::is_nothrow_move_constructible_v<Ray>);
static_assert(std::is_nothrow_move_assignable_v<Ray>);
static_assert(std::is_nothrow_move_constructible_v<StaticObject>);
static_assert(std::is_nothrow_move_constructible_v<DynamicObject>);

// Plain data: copying is a memcpy
static_assert(std::is_trivially_copyable_v<Position2D>);

// Owners of SDL resources: a copy would free them twice
static_assert(kPinned<RendererContext>);
static_assert(kPinned<ui::Ui>);
static_assert(kPinned<Game>);

// Their states hold a pointer back to them
static_assert(kPinned<Weapon>);
static_assert(kPinned<Enemy>);

// Interfaces cannot be copied through the base (it would slice)
static_assert(!std::is_copy_constructible_v<IGameObject>);
static_assert(!std::is_copy_constructible_v<ICharacter>);

TEST(TypeTraits, CheckedAtCompileTime) {
	SUCCEED();
}

}  // namespace
}  // namespace karakale
