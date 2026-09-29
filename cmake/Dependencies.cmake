# Third-party dependencies, exposed as wolfenstein::* targets so modules do not
# care where a library comes from.

include(FetchContent)

# ---- SDL3 and its satellite libraries -----------------------------------
# Built from source on every platform, at the same pinned versions: the
# browser has no SDL3_image or SDL3_mixer port, and system packages lag
# behind (Ubuntu 26.04 ships SDL 3.4.2 and no SDL3_mixer). Static libraries
# with only what the game uses, for a smaller WebAssembly build and a shorter
# compile. SYSTEM keeps the project's strict warnings out of their headers.

# Sets each dependency option named to its value, over any earlier choice
function(wolfenstein_dependency_options)
    set(options ${ARGN})
    while(options)
        list(POP_FRONT options name value)
        set(${name} ${value} CACHE BOOL "" FORCE)
    endwhile()
endfunction()

wolfenstein_dependency_options(BUILD_SHARED_LIBS OFF)

FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
    SYSTEM
)
# Windows, rendering and sound; no GPU API, gamepads, camera, sensors,
# dialogs or tray
wolfenstein_dependency_options(
    SDL_SHARED OFF SDL_STATIC ON
    SDL_TEST_LIBRARY OFF SDL_TESTS OFF SDL_EXAMPLES OFF
    SDL_GPU OFF SDL_CAMERA OFF SDL_JOYSTICK OFF SDL_HAPTIC OFF SDL_HIDAPI OFF
    SDL_POWER OFF SDL_SENSOR OFF SDL_DIALOG OFF SDL_TRAY OFF
    SDL_VULKAN OFF SDL_DISKAUDIO OFF
)
FetchContent_MakeAvailable(SDL3)

# PNG only (every picture the game has), through stb_image
FetchContent_Declare(SDL3_image
    URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.6/SDL3_image-3.4.6.tar.gz
    URL_HASH SHA256=d2e4637ae700f72e5196b8fbd749850ed2e5e1e09c5a5be8d06ff55aaccf3b01
    SYSTEM
)
wolfenstein_dependency_options(
    SDLIMAGE_VENDORED OFF SDLIMAGE_SAMPLES OFF SDLIMAGE_TESTS OFF
    SDLIMAGE_BACKEND_STB ON SDLIMAGE_BACKEND_IMAGEIO OFF
    SDLIMAGE_PNG ON SDLIMAGE_PNG_LIBPNG OFF SDLIMAGE_PNG_SAVE OFF
    SDLIMAGE_ANI OFF SDLIMAGE_AVIF OFF SDLIMAGE_BMP OFF SDLIMAGE_GIF OFF
    SDLIMAGE_JPG OFF SDLIMAGE_JXL OFF SDLIMAGE_LBM OFF SDLIMAGE_PCX OFF
    SDLIMAGE_PNM OFF SDLIMAGE_QOI OFF SDLIMAGE_SVG OFF SDLIMAGE_TGA OFF
    SDLIMAGE_TIF OFF SDLIMAGE_WEBP OFF SDLIMAGE_XCF OFF SDLIMAGE_XPM OFF
    SDLIMAGE_XV OFF
)
FetchContent_MakeAvailable(SDL3_image)

# SDL3_ttf draws with FreeType, which its release archive leaves out: built
# here too, with none of its optional libraries, and found by SDL3_ttf as if
# installed
FetchContent_Declare(freetype
    URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-14-3.tar.gz
    URL_HASH SHA256=dc49de6b01a266eef4876a4dd34d9842c475d3e28ff2eff63bd2fb760ab56261
    SYSTEM
    OVERRIDE_FIND_PACKAGE
)
wolfenstein_dependency_options(
    FT_DISABLE_ZLIB ON FT_DISABLE_BZIP2 ON FT_DISABLE_PNG ON
    FT_DISABLE_HARFBUZZ ON FT_DISABLE_BROTLI ON
)
FetchContent_MakeAvailable(freetype)
add_library(Freetype::Freetype ALIAS freetype)

FetchContent_Declare(SDL3_ttf
    URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz
    URL_HASH SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
    SYSTEM
)
wolfenstein_dependency_options(
    SDLTTF_VENDORED OFF SDLTTF_SAMPLES OFF
    SDLTTF_HARFBUZZ OFF SDLTTF_PLUTOSVG OFF
)
FetchContent_MakeAvailable(SDL3_ttf)

# The music is MP3, through dr_mp3; the sound effects are WAV files, which
# SDL itself reads
FetchContent_Declare(SDL3_mixer
    URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-3.2.4/SDL3_mixer-3.2.4.tar.gz
    URL_HASH SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
    SYSTEM
)
wolfenstein_dependency_options(
    SDLMIXER_VENDORED OFF SDLMIXER_TESTS OFF SDLMIXER_EXAMPLES OFF
    SDLMIXER_MP3 ON SDLMIXER_MP3_DRMP3 ON SDLMIXER_MP3_MPG123 OFF
    SDLMIXER_WAVE OFF SDLMIXER_AIFF OFF SDLMIXER_VOC OFF SDLMIXER_AU OFF
    SDLMIXER_FLAC OFF SDLMIXER_GME OFF SDLMIXER_MOD OFF SDLMIXER_MIDI OFF
    SDLMIXER_OPUS OFF SDLMIXER_VORBIS_STB OFF SDLMIXER_VORBIS_VORBISFILE OFF
    SDLMIXER_VORBIS_TREMOR OFF SDLMIXER_WAVPACK OFF
)
FetchContent_MakeAvailable(SDL3_mixer)

# Debug builds debug the game, not SDL: the libraries are optimised in every
# build, as a system's are. The tests and headless runs draw through SDL's
# software renderer, several times slower unoptimised.
foreach(target SDL3-static SDL3_image-static freetype SDL3_ttf-static SDL3_mixer-static)
    target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:-O2>)
endforeach()

add_library(wolfenstein_sdl3 INTERFACE)
target_link_libraries(wolfenstein_sdl3 INTERFACE SDL3::SDL3)
add_library(wolfenstein_sdl3_image INTERFACE)
target_link_libraries(wolfenstein_sdl3_image INTERFACE SDL3_image::SDL3_image)
add_library(wolfenstein_sdl3_ttf INTERFACE)
target_link_libraries(wolfenstein_sdl3_ttf INTERFACE SDL3_ttf::SDL3_ttf)
add_library(wolfenstein_sdl3_mixer INTERFACE)
target_link_libraries(wolfenstein_sdl3_mixer INTERFACE SDL3_mixer::SDL3_mixer)
add_library(wolfenstein::sdl3 ALIAS wolfenstein_sdl3)
add_library(wolfenstein::sdl3_image ALIAS wolfenstein_sdl3_image)
add_library(wolfenstein::sdl3_ttf ALIAS wolfenstein_sdl3_ttf)
add_library(wolfenstein::sdl3_mixer ALIAS wolfenstein_sdl3_mixer)

# ---- nlohmann/json (header only, pinned) ----------------------------------
# SYSTEM keeps the project's strict warnings out of third-party headers
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)
