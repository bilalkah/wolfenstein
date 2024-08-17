link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(
    texture_manager
    STATIC
    src/texture_manager.cpp
)

target_include_directories(texture_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    texture_manager
    PUBLIC
    SDL2
    SDL2_image
)