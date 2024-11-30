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

add_library(
    simple_weapon
    STATIC
    src/simple_weapon.cpp
)
target_include_directories(simple_weapon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(
    simple_weapon
    PUBLIC
    shooting_manager
    ray
)