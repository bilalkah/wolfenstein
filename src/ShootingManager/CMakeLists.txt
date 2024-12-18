add_library(
    shooting_manager
    STATIC
    src/shooting_manager.cpp
)
target_include_directories(shooting_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    shooting_manager
    PUBLIC
    single_raycaster
    vector
    character
    scene
)
