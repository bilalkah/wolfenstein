add_library(
    navigation_manager
    STATIC
    src/navigation_manager.cpp
    src/grid_path_finder.cpp
)
target_include_directories(navigation_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    navigation_manager
    PUBLIC
    vector
    character
    scene
)
