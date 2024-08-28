add_library(
    vector
    STATIC
    src/vector.cpp
)
target_include_directories(vector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
