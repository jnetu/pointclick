cmake_minimum_required(VERSION 3.26)
project(pointclick VERSION 0.1.0 LANGUAGES CXX)

include(cmake/CPM.cmake)
include(cmake/Dependencies.cmake)

# Shared game code: the executable and the tests use the same implementation.
add_library(pointclick_core STATIC
        src/game/Animation.cpp
        src/game/FloatingText.cpp
        src/game/Game.cpp
        src/game/Navigation.cpp
        src/game/ObjectAnimations.cpp
        src/game/Player.cpp
        src/game/Room.cpp
        src/game/SpriteClip.cpp
        src/game/RoomParameters.cpp
        src/content/RoomPresets.cpp
        src/content/RoomFiles.cpp
        src/content/SpriteFields.cpp
        src/debug/DebugEditor.cpp)
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
    if(MSVC)
        target_compile_options(pointclick_tests PRIVATE /UNDEBUG)
        target_compile_options(pointclick_editor_tests PRIVATE /UNDEBUG)
        target_compile_options(pointclick_sprite_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(pointclick_tests PRIVATE -UNDEBUG)
        target_compile_options(pointclick_editor_tests PRIVATE -UNDEBUG)
        target_compile_options(pointclick_sprite_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME pointclick_tests COMMAND pointclick_tests)
    add_test(NAME pointclick_editor_tests COMMAND pointclick_editor_tests)
    add_test(NAME pointclick_sprite_tests COMMAND pointclick_sprite_tests)
endif()

add_custom_command(TARGET pointclick POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:pointclick>/assets"
        COMMENT "Copying game assets")
