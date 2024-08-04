cmake_minimum_required(VERSION 3.16)  
project(wolfenstein VERSION 1.0 LANGUAGES CXX)
add_definitions("-Wall" "-g")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE wolfenstein)
set(PROJECT WOLFENSTEIN)
set(RESOURCE_DIR ${PROJECT_SOURCE_DIR}/assets/)

add_definitions(
    -DRESOURCE_DIR="${RESOURCE_DIR}"
)
# define debug mode
set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE Release)

add_subdirectory(app)
add_subdirectory(src/Camera)
add_subdirectory(src/CollisionManager)
add_subdirectory(src/Core)
add_subdirectory(src/Characters)
add_subdirectory(src/GameObjects)
add_subdirectory(src/Graphics)
add_subdirectory(src/Map)
add_subdirectory(src/Math)
add_subdirectory(src/TextureManager)
add_subdirectory(src/TimeManager)
add_subdirectory(src/Utilities)

find_package(SDL2 REQUIRED)

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

