FetchContent_Declare(googletest
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    SYSTEM
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(unit_tests
    math_test.cpp
    map_test.cpp
    raycaster_test.cpp
    state_machine_test.cpp
    lifetime_test.cpp
    line_of_sight_test.cpp
    type_traits_test.cpp
    grid_path_finder_test.cpp
    enemy_state_test.cpp
    monotonic_arena_test.cpp
    object_pool_test.cpp
    level_data_test.cpp
    animation_test.cpp
    world_test.cpp
    menu_test.cpp
)
target_link_libraries(unit_tests PRIVATE
    GTest::gtest_main
    vector
    map
    camera
    weapon
    character
    single_raycaster
    scene
    game
    navigation_manager
    profiler
    memory
    scene_loader
    animation
    world
    renderer_menu
)
# Counting heap allocations replaces global operator new, which clashes with
# the sanitizers' allocator, so allocation tests run in non-sanitizer builds
if(NOT WOLFENSTEIN_SANITIZERS)
    target_sources(unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/app/allocation_counter.cpp)
    target_compile_definitions(unit_tests PRIVATE WOLFENSTEIN_COUNTS_ALLOCATIONS)
endif()

# A test that hangs (e.g. an unbounded loop) fails instead of stalling CI
gtest_discover_tests(unit_tests PROPERTIES TIMEOUT 10)
