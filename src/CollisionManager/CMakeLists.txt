add_library(
    collision_manager
    STATIC
    src/collision_manager.cpp
)
target_include_directories(collision_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    collision_manager
    PUBLIC
    map
    vector
    game_object
)
