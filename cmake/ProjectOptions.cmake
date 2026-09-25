# Build settings shared by every first-party target, applied through the
# wolfenstein::options interface target.

option(WOLFENSTEIN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(WOLFENSTEIN_SANITIZERS "" CACHE STRING
    "Comma-separated sanitizers for native builds, e.g. address,undefined")

add_library(wolfenstein_options INTERFACE)
add_library(wolfenstein::options ALIAS wolfenstein_options)

target_compile_features(wolfenstein_options INTERFACE cxx_std_23)

# Where the game reads its assets: the web build packages them into a
# virtual file system mounted at /assets
if(EMSCRIPTEN)
    target_compile_definitions(wolfenstein_options INTERFACE RESOURCE_DIR="/assets/")
else()
    target_compile_definitions(wolfenstein_options INTERFACE
        RESOURCE_DIR="${PROJECT_SOURCE_DIR}/assets/")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(wolfenstein_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wnull-dereference
        -Wimplicit-fallthrough
        $<$<BOOL:${WOLFENSTEIN_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif()

# Browsers only allow threads on cross-origin isolated pages, so the web build
# is single threaded
if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(wolfenstein_options INTERFACE Threads::Threads)
endif()

if(WOLFENSTEIN_SANITIZERS)
    if(EMSCRIPTEN)
        message(FATAL_ERROR "WOLFENSTEIN_SANITIZERS is for native builds")
    endif()
    target_compile_options(wolfenstein_options INTERFACE
        -fsanitize=${WOLFENSTEIN_SANITIZERS} -fno-omit-frame-pointer
        -fno-sanitize-recover=all)
    target_link_options(wolfenstein_options INTERFACE
        -fsanitize=${WOLFENSTEIN_SANITIZERS})
endif()

# Links wolfenstein::options into every target defined in the project's own
# directories (app/, src/, tests/), recursively, so module CMakeLists stay
# free of boilerplate. Downloaded dependencies are left alone.
function(wolfenstein_apply_options_to_project)
    get_property(subdirs DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY SUBDIRECTORIES)
    foreach(dir ${subdirs})
        cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${dir}" NORMALIZE inside_project)
        if(inside_project)
            wolfenstein_apply_options(${dir})
        endif()
    endforeach()
endfunction()

function(wolfenstein_apply_options)
    foreach(dir ${ARGN})
        get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target ${targets})
            # Set through the properties rather than target_link_libraries,
            # which cannot mix with the plain signature some modules use
            get_target_property(type ${target} TYPE)
            if(type STREQUAL "INTERFACE_LIBRARY")
                set_property(TARGET ${target} APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES wolfenstein::options)
            else()
                set_property(TARGET ${target} APPEND PROPERTY
                    LINK_LIBRARIES wolfenstein::options)
            endif()
        endforeach()
        get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
        if(subdirs)
            wolfenstein_apply_options(${subdirs})
        endif()
    endforeach()
endfunction()
