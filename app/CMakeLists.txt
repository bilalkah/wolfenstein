add_executable(${EXECUTABLE} main.cpp)
# The allocation counter replaces global operator new/delete, which clashes
# with the sanitizers' own allocator; sanitizer builds track memory themselves
if(NOT WOLFENSTEIN_SANITIZERS)
    target_sources(${EXECUTABLE} PRIVATE allocation_counter.cpp)
endif()
target_compile_definitions(${EXECUTABLE} PRIVATE DEBUG=0)
target_link_libraries(
    ${EXECUTABLE} 
    game
    profiler
)

if(EMSCRIPTEN)
    # Emit index.html/.js/.wasm/.data, a static site that can be hosted as is
    set_target_properties(${EXECUTABLE} PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    target_link_options(${EXECUTABLE} PRIVATE
        "SHELL:--preload-file ${PROJECT_SOURCE_DIR}/assets@/assets"
        "SHELL:--shell-file ${PROJECT_SOURCE_DIR}/web/shell.html"
        -sALLOW_MEMORY_GROWTH=1
    )
    # Repackage when the shell page or any asset changes
    file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/assets/*)
    set_property(TARGET ${EXECUTABLE} APPEND PROPERTY LINK_DEPENDS
        ${PROJECT_SOURCE_DIR}/web/shell.html ${ASSET_FILES})
endif()
