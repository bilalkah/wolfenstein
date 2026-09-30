add_library(
    sound_manager
    STATIC
    src/sound_manager.cpp
    src/spatial_mixer.cpp
)
target_include_directories(sound_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    sound_manager
    PUBLIC
    karakale::sdl3
    karakale::sdl3_mixer
    vector
)
