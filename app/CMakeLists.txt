add_executable(${EXECUTABLE} main.cpp)
target_compile_definitions(${EXECUTABLE} PRIVATE DEBUG=1)
target_link_libraries(
    ${EXECUTABLE} 
    game
)
