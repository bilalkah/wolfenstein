add_library(
    map
    STATIC
    src/map.cpp
)
target_include_directories(map PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    map
    PUBLIC
    vector
)
