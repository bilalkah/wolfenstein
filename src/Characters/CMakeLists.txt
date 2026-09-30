# A player's command alone, for what sends and receives them (the network)
add_library(player_command INTERFACE)
target_include_directories(player_command INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(
    character
    STATIC
    src/player.cpp
    src/player_figure.cpp
    src/enemy.cpp
)
target_include_directories(character PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    character
    PUBLIC
    animation
    collision_manager
    enemy_state
    game_object
    karakale::sdl3
    vector
    weapon
    camera
    single_raycaster
    simple_weapon
    sound_manager
    shooting_manager
    scene
    profiler
    settings
)

