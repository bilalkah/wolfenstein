link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(
    scene
    STATIC
    src/scene.cpp
)

target_include_directories(scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    scene
    PUBLIC
    game_object
    map
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
)