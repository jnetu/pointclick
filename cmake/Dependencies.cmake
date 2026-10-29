option(POINTCLICK_PREFER_SYSTEM_SDL3
        "Use a locally installed SDL3 (>= 3.4) before fetching and building SDL3 from source"
        ON)

if(POINTCLICK_PREFER_SYSTEM_SDL3 AND NOT TARGET SDL3::SDL3 AND NOT CMAKE_CROSSCOMPILING)
    set(_pointclick_sdl3_hints)
    if(APPLE)
        list(APPEND _pointclick_sdl3_hints /opt/homebrew/opt/sdl3 /opt/homebrew /usr/local)
    endif()
    find_package(SDL3 3.4 CONFIG QUIET HINTS ${_pointclick_sdl3_hints})
    unset(_pointclick_sdl3_hints)
    if(TARGET SDL3::SDL3)
        message(STATUS "PointClick: using preinstalled SDL3 ${SDL3_VERSION}")
    endif()
endif()

if(NOT TARGET SDL3::SDL3)
    include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
    CPMAddPackage(
            NAME SDL3
            VERSION 3.4.16
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
            URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
            OPTIONS
            "SDL_SHARED OFF"
            "SDL_STATIC ON"
            "SDL_TEST_LIBRARY OFF"
            "SDL_TESTS OFF"
            "SDL_EXAMPLES OFF"
            "SDL_DISABLE_INSTALL ON"
            "SDL_CAMERA OFF"
            "SDL_DIALOG OFF"
            "SDL_DISKAUDIO OFF"
            "SDL_GPU OFF"
            "SDL_HAPTIC OFF"
            "SDL_OFFSCREEN OFF"
            "SDL_POWER OFF"
            "SDL_RENDER_GPU OFF"
            "SDL_RENDER_VULKAN OFF"
            "SDL_SENSOR OFF"
            "SDL_TRAY OFF"
            "SDL_VULKAN OFF"
    )
endif()
