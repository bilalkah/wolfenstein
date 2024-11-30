add_library(
    renderer_interface
    STATIC
    src/renderer_interface.cpp
)
target_include_directories(renderer_interface PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    renderer_interface
    PUBLIC
    camera
    scene
    texture_manager
    SDL2
    SDL2_image
    SDL2_ttf
)

add_library(
    renderer_2d
    STATIC
    src/renderer_2d.cpp
)
target_include_directories(renderer_2d PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    renderer_2d
    PUBLIC
    renderer_interface
)

add_library(
    renderer_3d
    STATIC
    src/renderer_3d.cpp
)
target_include_directories(renderer_3d PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    renderer_3d
    PUBLIC
    renderer_interface
)
