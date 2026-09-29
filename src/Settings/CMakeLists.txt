add_library(
    settings
    STATIC
    src/settings.cpp
    src/storage.cpp
    src/saved_game.cpp
)
target_include_directories(settings PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    settings
    PUBLIC
    wolfenstein::sdl3
)
