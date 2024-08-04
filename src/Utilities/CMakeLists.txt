link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(utilities
    src/logger.cpp
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Specify include directories for this library
target_include_directories(utilities PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
