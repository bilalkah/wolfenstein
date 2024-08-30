add_library(
    shotgun
    STATIC
    src/shotgun.cpp
)
target_include_directories(shotgun PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    shotgun
    PUBLIC
    animation
    time_manager
    texture_manager
)

