add_library(
    memory
    STATIC
    src/monotonic_arena.cpp
)
target_include_directories(memory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
