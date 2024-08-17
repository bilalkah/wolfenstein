link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


add_library(
    game
    STATIC
    src/game.cpp
)

target_include_directories(game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    game
    PUBLIC
    scene
    time_manager
    collision_manager
    texture_manager
    camera
    character
    game_object
    map
    vector
    renderer
    SDL2
)