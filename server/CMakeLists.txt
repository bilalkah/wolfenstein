# The multiplayer server: GameServer behind uWebSockets
add_executable(wolfenstein-server main.cpp)
target_link_libraries(wolfenstein-server PRIVATE
    game_server
    wolfenstein::uwebsockets
)
