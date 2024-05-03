cmake_minimum_required(VERSION 3.16)  
project(wolfenstein VERSION 1.0 LANGUAGES CXX)
add_definitions("-Wall" "-g")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE wolfenstein)
set(PROJECT WOLFENSTEIN)
set(RESOURCE_DIR ${PROJECT_SOURCE_DIR}/resources)

# define debug mode
set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE Release)


find_package(SDL2 REQUIRED)
file(GLOB_RECURSE ${PROJECT}_SOURCES "src/*.cpp" "include/*.h")
message(STATUS "SOURCES: ${${PROJECT}_SOURCES}")

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
)

add_executable(${EXECUTABLE} main.cpp ${${PROJECT}_SOURCES})
target_compile_definitions(${EXECUTABLE} PRIVATE DEBUG=1)
target_link_libraries(${EXECUTABLE} ${LIBRARIES})
