add_library(utilities
    src/logger.cpp
)
target_include_directories(utilities PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(uuid_generator
    src/uuid_generator.cpp
)
target_include_directories(uuid_generator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third-party
)