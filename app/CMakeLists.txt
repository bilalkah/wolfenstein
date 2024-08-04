include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
)
set(LIBRARIES
    ${SDL2_LIBRARIES}
    SDL2_ttf
    SDL2_image
)

add_executable(${EXECUTABLE} main.cpp)
target_compile_definitions(${EXECUTABLE} PRIVATE DEBUG=1)
target_link_libraries(
    ${EXECUTABLE} 
    ${LIBRARIES}
    game
)