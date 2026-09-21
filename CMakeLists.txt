cmake_minimum_required(VERSION 3.16)  
project(wolfenstein VERSION 1.0 LANGUAGES CXX C)
add_definitions("-Wall" "-g")
add_definitions("-Wno-dev")

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the output directory for the build executables and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# Browsers only allow threads on pages served with cross-origin isolation
# headers, so the web build is single threaded
if(NOT EMSCRIPTEN)
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

# Set the executable name
set(EXECUTABLE wolfenstein)
set(PROJECT WOLFENSTEIN)

# Set the resource directory. The web build packages assets into a virtual
# file system mounted at /assets
if(EMSCRIPTEN)
    set(RESOURCE_DIR /assets/)
else()
    set(RESOURCE_DIR ${PROJECT_SOURCE_DIR}/assets/)
endif()
add_definitions(
    -DRESOURCE_DIR="${RESOURCE_DIR}"
)

# Find Git
find_package(Git QUIET)

if(EMSCRIPTEN)
    # Emscripten ships SDL2 and its satellite libraries as ports; expose them
    # under the same names the modules link against
    set(SDL_PORTS
        "SDL2|-sUSE_SDL=2"
        "SDL2_image|-sUSE_SDL_IMAGE=2|-sSDL2_IMAGE_FORMATS=png,jpg"
        "SDL2_ttf|-sUSE_SDL_TTF=2"
        "SDL2_mixer|-sUSE_SDL_MIXER=2|-sSDL2_MIXER_FORMATS=mp3"
    )
    foreach(PORT ${SDL_PORTS})
        string(REPLACE "|" ";" PORT ${PORT})
        list(POP_FRONT PORT PORT_TARGET)
        add_library(${PORT_TARGET} INTERFACE)
        target_compile_options(${PORT_TARGET} INTERFACE ${PORT})
        target_link_options(${PORT_TARGET} INTERFACE ${PORT})
    endforeach()
else()
    find_package(SDL2 REQUIRED)
endif()

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules if any
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
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
# Only the A* planner is used from path-planning. Its own CMake project would
# also configure its vendored SDL3, yaml-cpp and googletest, so the three
# libraries A* needs are defined here instead.
set(PLANNING_DIR ${PROJECT_SOURCE_DIR}/third-party/path-planning/planning)
add_library(common_planning STATIC ${PLANNING_DIR}/utility/common_planning.cpp)
target_include_directories(common_planning PUBLIC ${PLANNING_DIR}/utility)
add_library(common_grid_base STATIC ${PLANNING_DIR}/utility/common_grid_base.cpp)
target_link_libraries(common_grid_base PUBLIC common_planning)
add_library(astar STATIC ${PLANNING_DIR}/grid_base/astar/astar.cpp)
target_include_directories(astar PUBLIC ${PLANNING_DIR} ${PLANNING_DIR}/grid_base/astar)
target_link_libraries(astar PUBLIC common_grid_base)
