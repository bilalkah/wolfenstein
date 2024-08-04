link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Game object library
add_library(
    game_object
    STATIC
    src/game_object.cpp
    src/static_object.cpp
    src/dynamic_object.cpp
)

# Specify include directories for this library
target_include_directories(game_object PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    game_object
    PUBLIC
    vector
    character
)
