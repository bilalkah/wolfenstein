# What the game's server and its players say to each other: the messages
# and how they are packed (the WebSocket connections carrying them are
# libraries': uWebSockets on the server, the browser's own and IXWebSocket
# in the game)
add_library(
    net
    STATIC
    src/protocol.cpp
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
