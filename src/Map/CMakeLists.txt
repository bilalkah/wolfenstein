add_library(
    map
    STATIC
    src/map.cpp
)

target_include_directories(map PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third-party/
)

target_link_directories(map PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third-party/path-planning/planning/utility
)

target_link_libraries(
    map
    PUBLIC
    common_planning
)