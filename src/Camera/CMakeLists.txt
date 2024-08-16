link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}
)

# Ray library
add_library(
    ray
    STATIC
    src/ray.cpp
)
target_include_directories(ray PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(
    ray
    PUBLIC
    vector
)

# camera library
add_library(
    camera
    STATIC
    src/camera.cpp
    src/raycaster.cpp
)
target_include_directories(camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}
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
    texture_manager
)
