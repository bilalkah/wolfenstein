add_library(
    animation
    STATIC
    src/time_based_single_animation.cpp
    src/walk_animation.cpp
)
target_include_directories(animation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
