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


find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third-party/uuid_v4/CMakeLists.txt")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

# internal libraries
add_subdirectory(app)
add_subdirectory(src/Animation)
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
add_subdirectory(src/Utility)

# external libraries
add_subdirectory(third-party/uuid_v4)

find_package(SDL2 REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/third-party
    ${SDL2_INCLUDE_DIRS}
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/third-party
)
set(LIBRARIES
    ${SDL2_LIBRARIES}
    SDL2_ttf
    SDL2_image
)

