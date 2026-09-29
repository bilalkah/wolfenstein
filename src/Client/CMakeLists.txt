# A player's side of a multiplayer game
add_library(
    match_client
    STATIC
    src/match_client.cpp
)
target_include_directories(match_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    match_client
    PUBLIC
    world
    net
)
