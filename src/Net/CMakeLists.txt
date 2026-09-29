# What the game's server and its players say to each other: the messages
# and how they are packed, and a player's connection to the server (the
# WebSocket itself is a library's: uWebSockets on the server, the browser's
# own and IXWebSocket in the game)
add_library(
    net
    STATIC
    src/protocol.cpp
    src/connection.cpp
)
target_include_directories(net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    net
    PUBLIC
    player_command
    vector
)
if(EMSCRIPTEN)
    target_link_options(net PUBLIC -lwebsocket.js)
else()
    target_link_libraries(net PRIVATE ixwebsocket)
endif()
