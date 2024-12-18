cmake_minimum_required(VERSION 3.5)  
project(wolfenstein VERSION 1.0 LANGUAGES CXX C)
add_definitions("-Wall" "-g")
add_definitions("-Wno-dev")

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the output directory for the build executables and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Set the executable name
set(EXECUTABLE wolfenstein)
set(PROJECT WOLFENSTEIN)

# Set the resource directory
set(RESOURCE_DIR ${PROJECT_SOURCE_DIR}/assets/)
add_definitions(
    -DRESOURCE_DIR="${RESOURCE_DIR}"
)

# Find Git
find_package(Git QUIET)
find_package(SDL2 REQUIRED)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules if any
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

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third-party/path-planning/CMakeLists.txt")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

# internal libraries/modules
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
add_subdirectory(src/NavigationManager)
add_subdirectory(src/ShootingManager)
add_subdirectory(src/Strike)
add_subdirectory(src/State)
add_subdirectory(src/SoundManager)
add_subdirectory(src/TextureManager)
add_subdirectory(src/TimeManager)
add_subdirectory(src/Utility)

# external libraries/modules
add_subdirectory(third-party/uuid_v4)
add_subdirectory(third-party/path-planning)
