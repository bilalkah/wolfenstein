# Build settings shared by every first-party target, applied through the
# karakale::options interface target.

option(KARAKALE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(KARAKALE_SANITIZERS "" CACHE STRING
    "Comma-separated sanitizers for native builds, e.g. address,undefined")

add_library(karakale_options INTERFACE)
add_library(karakale::options ALIAS karakale_options)

target_compile_features(karakale_options INTERFACE cxx_std_23)

# Where the game reads its assets: the web build packages them into a
# virtual file system mounted at /assets
if(EMSCRIPTEN)
    target_compile_definitions(karakale_options INTERFACE RESOURCE_DIR="/assets/")
else()
    target_compile_definitions(karakale_options INTERFACE
        RESOURCE_DIR="${PROJECT_SOURCE_DIR}/assets/")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(karakale_options INTERFACE
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
        $<$<BOOL:${KARAKALE_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif()

# Browsers only allow threads on cross-origin isolated pages, so the web build
# is single threaded
if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(karakale_options INTERFACE Threads::Threads)
endif()

if(KARAKALE_SANITIZERS)
    if(EMSCRIPTEN)
        message(FATAL_ERROR "KARAKALE_SANITIZERS is for native builds")
    endif()
    target_compile_options(karakale_options INTERFACE
        -fsanitize=${KARAKALE_SANITIZERS} -fno-omit-frame-pointer
        -fno-sanitize-recover=all)
    target_link_options(karakale_options INTERFACE
        -fsanitize=${KARAKALE_SANITIZERS})
endif()

# Links karakale::options into every target defined in the project's own
# directories (app/, src/, tests/), recursively, so module CMakeLists stay
# free of boilerplate. Downloaded dependencies are left alone.
function(karakale_apply_options_to_project)
    get_property(subdirs DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY SUBDIRECTORIES)
    karakale_apply_options(${subdirs})
endfunction()

# Applies the options to every target in the given directories and their
# subdirectories, skipping anything outside the source tree and fetched
# dependencies (GoogleTest, Google Benchmark): those live under the build
# directory, which may itself be inside the source tree, and keep their own
# flags rather than the project's strict warnings
function(karakale_apply_options)
    foreach(dir ${ARGN})
        cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${dir}" NORMALIZE inside_project)
        cmake_path(IS_PREFIX PROJECT_BINARY_DIR "${dir}" NORMALIZE in_build_dir)
        if(NOT inside_project OR in_build_dir)
            continue()
        endif()
        get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target ${targets})
            # Set through the properties rather than target_link_libraries,
            # which cannot mix with the plain signature some modules use
            get_target_property(type ${target} TYPE)
            if(type STREQUAL "INTERFACE_LIBRARY")
                set_property(TARGET ${target} APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES karakale::options)
            else()
                set_property(TARGET ${target} APPEND PROPERTY
                    LINK_LIBRARIES karakale::options)
            endif()
        endforeach()
        get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
        if(subdirs)
            karakale_apply_options(${subdirs})
        endif()
    endforeach()
endfunction()
