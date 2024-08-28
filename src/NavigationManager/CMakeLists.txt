add_library(
    navigation_manager
    STATIC
    src/navigation_manager.cpp
)
target_include_directories(navigation_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third-party/
)
target_link_libraries(
    navigation_manager
    PUBLIC
    vector
    character
    astar
    map
)
