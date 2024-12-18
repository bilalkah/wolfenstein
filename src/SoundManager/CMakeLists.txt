add_library(
    sound_manager
    STATIC
    src/sound_manager.cpp
)
target_include_directories(sound_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    sound_manager
    PUBLIC
    SDL2
    SDL2_mixer
)
