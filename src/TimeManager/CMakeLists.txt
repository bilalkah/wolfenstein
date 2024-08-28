add_library(
    time_manager
    STATIC
    src/time_manager.cpp
)
target_include_directories(time_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
