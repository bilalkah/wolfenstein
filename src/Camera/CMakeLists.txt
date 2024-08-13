link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


add_library(
    ray
    STATIC
    src/ray.cpp
)

# Specify include directories for this library
target_include_directories(ray PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    ray
    PUBLIC
    vector
)

# Game object library
add_library(
    camera
    STATIC
    src/camera.cpp
    src/raycaster.cpp
)

# Specify include directories for this library
target_include_directories(camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_libraries(
    camera
    PUBLIC
    game_object
    ray
    character
    vector
    map
    scene
)
