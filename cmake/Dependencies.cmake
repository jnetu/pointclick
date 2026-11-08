# Versão de referência do SDL3: a mesma para o pacote do sistema e para o CPM.
# Ao atualizar, troque a versão e o URL_HASH abaixo e revise as citações em docs/.
set(POINTCLICK_SDL3_VERSION 3.4.20)
set(POINTCLICK_SDL3_SHA256 f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f)

option(POINTCLICK_PREFER_SYSTEM_SDL3
        "Use a locally installed SDL3 (>= ${POINTCLICK_SDL3_VERSION}) before fetching and building SDL3 from source"
        ON)

set(POINTCLICK_SDL3_INCLUDE_DIRS "")

if(POINTCLICK_PREFER_SYSTEM_SDL3 AND NOT TARGET SDL3::SDL3 AND NOT CMAKE_CROSSCOMPILING)
    set(_pointclick_sdl3_hints)
    if(APPLE)
        list(APPEND _pointclick_sdl3_hints /opt/homebrew/opt/sdl3 /opt/homebrew /usr/local)
    endif()
    find_package(SDL3 ${POINTCLICK_SDL3_VERSION} CONFIG QUIET HINTS ${_pointclick_sdl3_hints})
    unset(_pointclick_sdl3_hints)
    if(TARGET SDL3::SDL3)
        if(TARGET SDL3::Headers)
            get_target_property(POINTCLICK_SDL3_INCLUDE_DIRS SDL3::Headers INTERFACE_INCLUDE_DIRECTORIES)
        else()
            get_target_property(POINTCLICK_SDL3_INCLUDE_DIRS SDL3::SDL3 INTERFACE_INCLUDE_DIRECTORIES)
        endif()
        if(POINTCLICK_SDL3_INCLUDE_DIRS)
            list(GET POINTCLICK_SDL3_INCLUDE_DIRS 0 _pointclick_first_sdl3_inc)
            if(EXISTS "${_pointclick_first_sdl3_inc}" AND NOT EXISTS "${CMAKE_BINARY_DIR}/_deps/sdl3-src/include")
                file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps/sdl3-src")
                file(CREATE_LINK "${_pointclick_first_sdl3_inc}"
                        "${CMAKE_BINARY_DIR}/_deps/sdl3-src/include" SYMBOLIC)
            endif()
            unset(_pointclick_first_sdl3_inc)
        endif()
        message(STATUS "PointClick: using preinstalled SDL3 ${SDL3_VERSION} (${POINTCLICK_SDL3_INCLUDE_DIRS})")
    elseif(SDL3_CONSIDERED_VERSIONS)
        set(_pointclick_sdl3_found_versions ${SDL3_CONSIDERED_VERSIONS})
        list(REMOVE_DUPLICATES _pointclick_sdl3_found_versions)
        message(STATUS "PointClick: SDL3 do sistema (${_pointclick_sdl3_found_versions}) e anterior a "
                "${POINTCLICK_SDL3_VERSION}; usando SDL3 ${POINTCLICK_SDL3_VERSION} via CPM "
                "(atualize o pacote do sistema para evitar a compilacao do SDL)")
        unset(_pointclick_sdl3_found_versions)
    endif()
endif()

if(NOT TARGET SDL3::SDL3)
    include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
    CPMAddPackage(
            NAME SDL3
            VERSION ${POINTCLICK_SDL3_VERSION}
            URL https://github.com/libsdl-org/SDL/releases/download/release-${POINTCLICK_SDL3_VERSION}/SDL3-${POINTCLICK_SDL3_VERSION}.tar.gz
            URL_HASH SHA256=${POINTCLICK_SDL3_SHA256}
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
