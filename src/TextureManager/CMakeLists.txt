add_library(
    texture_manager
    STATIC
    src/texture_manager.cpp
)
target_include_directories(texture_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    texture_manager
    PUBLIC
    karakale::sdl3
    karakale::sdl3_image
    PRIVATE
    nlohmann_json::nlohmann_json
)