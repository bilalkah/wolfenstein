add_library(
    animation
    STATIC
    src/looped_animation.cpp
    src/triggered_single_animation.cpp
)
target_include_directories(animation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    animation 
    PUBLIC
    texture_manager
)

