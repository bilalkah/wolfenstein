link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Math library
add_library(
    character
    STATIC
    src/player.cpp
    src/enemy.cpp
)

# Specify include directories for this library
target_include_directories(character PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    character
    PUBLIC
    game_object
    vector
    collision_manager
)


# Math library
add_library(
    player
    STATIC
    src/player.cpp
)

# Specify include directories for this library
target_include_directories(player PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    player
    PUBLIC
    character
    collision_manager
)