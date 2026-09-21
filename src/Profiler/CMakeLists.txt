add_library(
    profiler
    STATIC
    src/profiler.cpp
)
target_include_directories(profiler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
