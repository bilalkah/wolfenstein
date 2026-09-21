add_library(
    settings
    STATIC
    src/settings.cpp
)
target_include_directories(settings PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    settings
    PUBLIC
    SDL2
)
