add_library(
    ray
    STATIC
    src/ray.cpp
)
target_include_directories(ray PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    ray
    PUBLIC
    vector
)


add_library(
    camera
    STATIC
    src/camera.cpp
    src/raycaster.cpp
)
target_include_directories(camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    camera
    PUBLIC
    game_object
    ray
    scene
    vector
)

add_library(
    single_raycaster
    STATIC
    src/single_raycaster.cpp
)
target_include_directories(single_raycaster PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    single_raycaster
    PUBLIC
    ray
    vector
    map
)