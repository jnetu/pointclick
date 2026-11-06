# Fails when a layer includes a layer it must not depend on (run as a CTest test):
#   cmake -DSOURCE_DIR=<repo> -P cmake/CheckLayerIncludes.cmake
# game/ may include only game/ and the SDL value types (SDL_rect.h, SDL_pixels.h).
set(allowed_game "game")
set(allowed_content "content;game")
set(allowed_input "input;game")
set(allowed_ui "ui;input;game")
set(allowed_graphics "graphics;ui;input;game")
set(allowed_debug "debug;graphics;ui;input;content;game")

set(failures "")
foreach(layer game content input ui graphics debug)
    file(GLOB_RECURSE sources "${SOURCE_DIR}/src/${layer}/*.hpp" "${SOURCE_DIR}/src/${layer}/*.cpp")
    foreach(source IN LISTS sources)
        file(STRINGS "${source}" includes REGEX "^[ \t]*#[ \t]*include")
        foreach(line IN LISTS includes)
            if(line MATCHES "\"([a-z]+)/")
                list(FIND allowed_${layer} "${CMAKE_MATCH_1}" index)
                if(index EQUAL -1)
                    list(APPEND failures "${source}: ${line}")
                endif()
            elseif(layer STREQUAL "game" AND line MATCHES "<SDL3/"
                   AND NOT line MATCHES "<SDL3/SDL_(rect|pixels)\\.h>")
                list(APPEND failures "${source}: ${line} (game/ uses only SDL value types)")
            endif()
        endforeach()
    endforeach()
endforeach()

if(failures)
    list(JOIN failures "\n  " report)
    message(FATAL_ERROR "Forbidden layer includes:\n  ${report}")
endif()
message(STATUS "Layer includes OK")
