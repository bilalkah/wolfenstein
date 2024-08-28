add_library(
    game_object
    STATIC
    src/static_object.cpp
    src/dynamic_object.cpp
)
target_include_directories(game_object PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    game_object
    PUBLIC
    vector
    uuid_generator
    animation
)
