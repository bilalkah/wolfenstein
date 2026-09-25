add_library(utilities
    src/logger.cpp
)
target_include_directories(utilities PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
