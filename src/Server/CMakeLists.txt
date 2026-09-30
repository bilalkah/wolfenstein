# A multiplayer game as the server runs it, whatever carries its messages
add_library(
    game_server
    STATIC
    src/game_server.cpp
    src/lobby.cpp
    src/match_rules.cpp
)
target_include_directories(game_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    game_server
    PUBLIC
    world
    net
    texture_manager
)
