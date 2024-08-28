add_library(
    scene
    STATIC
    src/scene.cpp
)
target_include_directories(scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    scene
    PUBLIC
    game_object
    map
    navigation_manager
)

add_library(
    renderer
    STATIC
    src/renderer.cpp
)
target_include_directories(renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    renderer
    PUBLIC
    camera
    texture_manager
    navigation_manager
    SDL2
    SDL2_image
    SDL2_ttf
)
