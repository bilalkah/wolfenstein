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
    level_design_test.cpp
    simulation_test.cpp
    fixed_step_test.cpp
    aim_test.cpp
    pickup_test.cpp
    intel_test.cpp
    exploration_test.cpp
    door_test.cpp
    level_stats_test.cpp
    objective_test.cpp
    secret_test.cpp
    collision_test.cpp
    arsenal_test.cpp
    impact_test.cpp
    balance_test.cpp
    mouse_look_test.cpp
    hearing_test.cpp
    drop_test.cpp
    sound_test.cpp
    navigation_test.cpp
    enemy_view_test.cpp
    patrol_test.cpp
    tactics_test.cpp
    view_test.cpp
    spatial_sound_test.cpp
    story_test.cpp
    projectile_test.cpp
    saved_game_test.cpp
    settings_test.cpp
    texture_manifest_test.cpp
    players_test.cpp
    protocol_test.cpp
    game_server_test.cpp
    match_test.cpp
    match_url_test.cpp
    lobby_test.cpp
    addresses_test.cpp
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
    minimap
    renderer_3d
    time_manager
    shooting_manager
    collision_manager
    texture_manager
    settings
    net
    game_server
    match_client
)
# Counting heap allocations replaces global operator new, which clashes with
# the sanitizers' allocator, so allocation tests run in non-sanitizer builds
if(NOT KARAKALE_SANITIZERS)
    target_sources(unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/app/allocation_counter.cpp)
    target_compile_definitions(unit_tests PRIVATE KARAKALE_COUNTS_ALLOCATIONS)
endif()

# A test that hangs (e.g. an unbounded loop) fails instead of stalling CI.
# Under the sanitizers, leaks inside the graphics driver SDL loads are not
# the game's (see lsan.supp), and everything runs several times slower: a
# lobby filled with rooms, each loading the game, takes up to 12 s there.
if(KARAKALE_SANITIZERS)
    gtest_discover_tests(unit_tests PROPERTIES TIMEOUT 30 ENVIRONMENT
        "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")
else()
    gtest_discover_tests(unit_tests PROPERTIES TIMEOUT 10)
endif()
