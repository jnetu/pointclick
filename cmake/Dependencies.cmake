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

# SDL3_ttf (texto de jogo). Mesma regra do SDL3: uma versão de referência vale como mínimo para o
# pacote do sistema e como versão fixa do CPM. O tarball do SDL3_ttf não traz o FreeType embutido:
# usa o FreeType do sistema quando houver (desktop) e, senão (mobile, console, Windows sem pacote),
# baixa o FreeType fixado abaixo e o compila junto. HarfBuzz e plutosvg ficam desligados: texto
# latino com acentos não precisa de shaping complexo nem de emoji colorido.
set(POINTCLICK_SDL3_TTF_VERSION 3.2.2)
set(POINTCLICK_SDL3_TTF_SHA256 63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d)
set(POINTCLICK_FREETYPE_VERSION 2.13.3)
set(POINTCLICK_FREETYPE_SHA256 0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289)

option(POINTCLICK_PREFER_SYSTEM_SDL3_TTF
        "Use a locally installed SDL3_ttf (>= ${POINTCLICK_SDL3_TTF_VERSION}) before fetching and building it"
        ON)
option(POINTCLICK_VENDOR_FREETYPE
        "Build FreeType from source for SDL3_ttf even when the system has it"
        OFF)

if(POINTCLICK_PREFER_SYSTEM_SDL3_TTF AND NOT TARGET SDL3_ttf::SDL3_ttf AND NOT CMAKE_CROSSCOMPILING)
    set(_pointclick_ttf_hints)
    if(APPLE)
        list(APPEND _pointclick_ttf_hints /opt/homebrew/opt/sdl3_ttf /opt/homebrew /usr/local)
    endif()
    find_package(SDL3_ttf ${POINTCLICK_SDL3_TTF_VERSION} CONFIG QUIET HINTS ${_pointclick_ttf_hints})
    unset(_pointclick_ttf_hints)
    if(TARGET SDL3_ttf::SDL3_ttf)
        message(STATUS "PointClick: using preinstalled SDL3_ttf ${SDL3_ttf_VERSION}")
    endif()
endif()

if(NOT TARGET SDL3_ttf::SDL3_ttf)
    include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
    CPMAddPackage(
            NAME SDL3_ttf
            VERSION ${POINTCLICK_SDL3_TTF_VERSION}
            URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-${POINTCLICK_SDL3_TTF_VERSION}/SDL3_ttf-${POINTCLICK_SDL3_TTF_VERSION}.tar.gz
            URL_HASH SHA256=${POINTCLICK_SDL3_TTF_SHA256}
            DOWNLOAD_ONLY YES
    )

    set(_pointclick_vendor_freetype ${POINTCLICK_VENDOR_FREETYPE})
    if(NOT _pointclick_vendor_freetype)
        if(CMAKE_CROSSCOMPILING OR ANDROID OR IOS OR MSVC)
            set(_pointclick_vendor_freetype ON)
        else()
            find_package(Freetype QUIET)
            if(NOT FREETYPE_FOUND)
                set(_pointclick_vendor_freetype ON)
            endif()
        endif()
    endif()
    if(_pointclick_vendor_freetype)
        CPMAddPackage(
                NAME freetype
                VERSION ${POINTCLICK_FREETYPE_VERSION}
                URL https://download.savannah.gnu.org/releases/freetype/freetype-${POINTCLICK_FREETYPE_VERSION}.tar.xz
                URL_HASH SHA256=${POINTCLICK_FREETYPE_SHA256}
                DOWNLOAD_ONLY YES
        )
        # SDL3_ttf builds a vendored FreeType from external/freetype (a git submodule upstream).
        if(NOT EXISTS "${SDL3_ttf_SOURCE_DIR}/external/freetype/CMakeLists.txt")
            file(REMOVE_RECURSE "${SDL3_ttf_SOURCE_DIR}/external/freetype")
            file(CREATE_LINK "${freetype_SOURCE_DIR}" "${SDL3_ttf_SOURCE_DIR}/external/freetype"
                    SYMBOLIC COPY_ON_ERROR)
        endif()
        message(STATUS "PointClick: SDL3_ttf ${POINTCLICK_SDL3_TTF_VERSION} with vendored FreeType ${POINTCLICK_FREETYPE_VERSION}")
    else()
        message(STATUS "PointClick: SDL3_ttf ${POINTCLICK_SDL3_TTF_VERSION} with the system FreeType ${FREETYPE_VERSION_STRING}")
    endif()

    set(SDLTTF_VENDORED ${_pointclick_vendor_freetype} CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDLTTF_WERROR OFF CACHE BOOL "" FORCE)
    # Static like the CPM SDL3; BUILD_SHARED_LIBS only for this subdirectory.
    set(_pointclick_saved_shared ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory("${SDL3_ttf_SOURCE_DIR}" "${SDL3_ttf_BINARY_DIR}" EXCLUDE_FROM_ALL SYSTEM)
    set(BUILD_SHARED_LIBS ${_pointclick_saved_shared})
    unset(_pointclick_saved_shared)
    unset(_pointclick_vendor_freetype)
endif()
