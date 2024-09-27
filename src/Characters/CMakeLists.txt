add_library(
    character
    STATIC
    src/player.cpp
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
    SDL2
    vector
    weapon
)

