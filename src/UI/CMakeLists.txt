add_library(
    ui
    STATIC
    src/ui.cpp
)
target_include_directories(ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    ui
    PUBLIC
    wolfenstein::sdl2
    wolfenstein::sdl2_ttf
)
