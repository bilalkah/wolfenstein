cmake_minimum_required(VERSION 3.25)
project(wolfenstein VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(EXECUTABLE wolfenstein)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(ProjectOptions)
include(Dependencies)

# internal libraries/modules
add_subdirectory(app)
add_subdirectory(src/Animation)
add_subdirectory(src/Camera)
add_subdirectory(src/CollisionManager)
add_subdirectory(src/Core)
add_subdirectory(src/Characters)
add_subdirectory(src/GameObjects)
add_subdirectory(src/Graphics)
add_subdirectory(src/GameMap)
add_subdirectory(src/Math)
add_subdirectory(src/Net)
add_subdirectory(src/Allocators)
add_subdirectory(src/NavigationManager)
add_subdirectory(src/Profiler)
add_subdirectory(src/Settings)
add_subdirectory(src/ShootingManager)
add_subdirectory(src/Strike)
add_subdirectory(src/State)
add_subdirectory(src/SoundManager)
add_subdirectory(src/TextureManager)
add_subdirectory(src/TimeManager)
add_subdirectory(src/UI)

if(NOT EMSCRIPTEN)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(benchmarks/micro)
endif()

# Warnings, sanitizers and shared definitions for all first-party code
wolfenstein_apply_options_to_project()
