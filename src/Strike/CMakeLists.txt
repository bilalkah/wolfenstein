add_library(
    weapon
    STATIC
    src/weapon.cpp
)
target_include_directories(weapon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    weapon
    PUBLIC
    animation
    time_manager
    texture_manager
    weapon_state
)

