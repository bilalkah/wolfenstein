# The multiplayer server: GameServer behind uWebSockets
add_executable(karakale-server main.cpp)
target_link_libraries(karakale-server PRIVATE
    game_server
    karakale::uwebsockets
)
