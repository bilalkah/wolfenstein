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
    game_object
    vector
    collision_manager
    animation
    SDL2
    weapon
    enemy_state
)

