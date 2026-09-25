# Third-party dependencies, exposed as wolfenstein::* targets so modules do not
# care whether a library comes from the system, an Emscripten port or a
# download.

include(FetchContent)

# ---- SDL2 and its satellite libraries -----------------------------------
if(EMSCRIPTEN)
    # Emscripten builds SDL2 and friends as ports, selected by compile and
    # link flags
    function(wolfenstein_sdl_port target)
        add_library(${target} INTERFACE)
        target_compile_options(${target} INTERFACE ${ARGN})
        target_link_options(${target} INTERFACE ${ARGN})
    endfunction()
    wolfenstein_sdl_port(wolfenstein_sdl2 -sUSE_SDL=2)
    wolfenstein_sdl_port(wolfenstein_sdl2_image -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS=png,jpg)
    wolfenstein_sdl_port(wolfenstein_sdl2_ttf -sUSE_SDL_TTF=2)
    wolfenstein_sdl_port(wolfenstein_sdl2_mixer -sUSE_SDL_MIXER=2 -sSDL2_MIXER_FORMATS=mp3)
else()
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    find_package(SDL2_mixer CONFIG REQUIRED)
    add_library(wolfenstein_sdl2 INTERFACE)
    target_link_libraries(wolfenstein_sdl2 INTERFACE SDL2::SDL2)
    add_library(wolfenstein_sdl2_image INTERFACE)
    target_link_libraries(wolfenstein_sdl2_image INTERFACE SDL2_image::SDL2_image wolfenstein_sdl2)
    add_library(wolfenstein_sdl2_ttf INTERFACE)
    target_link_libraries(wolfenstein_sdl2_ttf INTERFACE SDL2_ttf::SDL2_ttf wolfenstein_sdl2)
    add_library(wolfenstein_sdl2_mixer INTERFACE)
    target_link_libraries(wolfenstein_sdl2_mixer INTERFACE SDL2_mixer::SDL2_mixer wolfenstein_sdl2)
endif()
add_library(wolfenstein::sdl2 ALIAS wolfenstein_sdl2)
add_library(wolfenstein::sdl2_image ALIAS wolfenstein_sdl2_image)
add_library(wolfenstein::sdl2_ttf ALIAS wolfenstein_sdl2_ttf)
add_library(wolfenstein::sdl2_mixer ALIAS wolfenstein_sdl2_mixer)

# ---- nlohmann/json (header only, pinned) ----------------------------------
# SYSTEM keeps the project's strict warnings out of third-party headers
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)
