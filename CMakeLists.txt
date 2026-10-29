cmake_minimum_required(VERSION 3.26)
project(pointclick VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(POINTCLICK_COMPILER_CACHE NAMES sccache ccache)
    if(POINTCLICK_COMPILER_CACHE)
        set(CMAKE_C_COMPILER_LAUNCHER "${POINTCLICK_COMPILER_CACHE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${POINTCLICK_COMPILER_CACHE}")
    endif()
endif()

include(cmake/Dependencies.cmake)

# Shared game code: the executable and the tests use the same implementation.
add_library(pointclick_core STATIC
        src/game/Animation.cpp
        src/game/Camera.cpp
        src/game/FloatingText.cpp
        src/game/Game.cpp
        src/game/Interaction.cpp
        src/game/Navigation.cpp
        src/game/ObjectAnimations.cpp
        src/game/Player.cpp
        src/game/Room.cpp
        src/game/SessionState.cpp
        src/game/SpriteClip.cpp
        src/game/RoomParameters.cpp
        src/content/InteractionFiles.cpp
        src/content/RoomPresets.cpp
        src/content/RoomFiles.cpp
        src/content/SpriteFields.cpp
        src/debug/DebugEditor.cpp
        src/input/InputSystem.cpp
        src/input/InputRouter.cpp)
target_compile_features(pointclick_core PUBLIC cxx_std_20)
target_include_directories(pointclick_core PUBLIC src)
target_link_libraries(pointclick_core PUBLIC SDL3::SDL3)

add_library(pointclick_graphics STATIC
        src/graphics/EntityRenderer.cpp
        src/graphics/Renderer.cpp
        src/graphics/RoomBackdropRenderer.cpp
        src/graphics/SceneRenderer.cpp
        src/graphics/SpriteLibrary.cpp
        src/graphics/DiagnosticsOverlay.cpp
        src/graphics/DebugOverlay.cpp)
target_link_libraries(pointclick_graphics PUBLIC pointclick_core)
add_executable(pointclick src/main.cpp src/core/Application.cpp)
target_link_libraries(pointclick PRIVATE pointclick_graphics)

foreach(target pointclick_core pointclick_graphics pointclick)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)
if(BUILD_TESTING)
    add_executable(pointclick_tests tests/GameTests.cpp)
    target_link_libraries(pointclick_tests PRIVATE pointclick_core)
    add_executable(pointclick_editor_tests tests/EditorTests.cpp)
    target_link_libraries(pointclick_editor_tests PRIVATE pointclick_graphics)
    add_executable(pointclick_sprite_tests tests/SpriteTests.cpp)
    target_link_libraries(pointclick_sprite_tests PRIVATE pointclick_graphics)
    target_compile_definitions(pointclick_editor_tests PRIVATE
            POINTCLICK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(pointclick_sprite_tests PRIVATE
            POINTCLICK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    # The regression checks must also run in Release builds.
    foreach(test_target pointclick_tests pointclick_editor_tests pointclick_sprite_tests)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /W4 /permissive- /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        endif()
    endforeach()
    add_test(NAME pointclick_tests COMMAND pointclick_tests)
    add_test(NAME pointclick_editor_tests COMMAND pointclick_editor_tests)
    add_test(NAME pointclick_sprite_tests COMMAND pointclick_sprite_tests)
endif()

add_custom_command(TARGET pointclick POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:pointclick>/assets"
        COMMENT "Copying game assets")
