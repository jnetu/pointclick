cmake_minimum_required(VERSION 3.26)
project(pointclick VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(POINTCLICK_COMPILER_CACHE NAMES sccache ccache)
    if(POINTCLICK_COMPILER_CACHE)
        set(CMAKE_C_COMPILER_LAUNCHER "${POINTCLICK_COMPILER_CACHE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${POINTCLICK_COMPILER_CACHE}")
    endif()
endif()

include(cmake/Dependencies.cmake)
# Optional format/format-check targets (changed lines only; outside ALL and CTest).
include(cmake/ClangFormat.cmake)

# One warning set for every target. /W4 already reports shadowing (C4456-C4459) and narrowing
# conversions (C4244, C4267), the MSVC counterparts of -Wshadow and -Wconversion.
if(MSVC)
    set(POINTCLICK_WARNINGS /W4 /permissive-)
    set(POINTCLICK_KEEP_ASSERTS /UNDEBUG)
else()
    set(POINTCLICK_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC's -Wextra flags every designated initializer that leaves a defaulted member out.
        list(APPEND POINTCLICK_WARNINGS -Wno-missing-field-initializers)
    endif()
    set(POINTCLICK_KEEP_ASSERTS -UNDEBUG)
endif()

option(POINTCLICK_ENABLE_TOOLS "In-game DEBUG editor and its overlay (OFF for store builds)" ON)

# One static library per layer, so the linker rejects a forbidden dependency:
#   game <- content, input <- ui <- graphics <- tools (optional) <- executable
# Domain: rules, definitions and state. No rendering, no file I/O.
add_library(pointclick_game STATIC
        src/game/ActionCatalog.cpp
        src/game/Autosave.cpp
        src/game/Animation.cpp
        src/game/Camera.cpp
        src/game/Dialogue.cpp
        src/game/FloatingText.cpp
        src/game/Game.cpp
        src/game/Interaction.cpp
        src/game/Navigation.cpp
        src/game/ObjectAnimations.cpp
        src/game/Player.cpp
        src/game/Room.cpp
        src/game/Scene.cpp
        src/game/ScriptRunner.cpp
        src/game/SessionState.cpp
        src/game/SpriteClip.cpp
        src/game/RoomParameters.cpp)
target_compile_features(pointclick_game PUBLIC cxx_std_20)
target_include_directories(pointclick_game PUBLIC src ${POINTCLICK_SDL3_INCLUDE_DIRS})
# The domain calls non-inline SDL_rect.h helpers; the include check below keeps it to value types.
target_link_libraries(pointclick_game PUBLIC SDL3::SDL3)

# Content files <-> validated definitions, asset layout.
add_library(pointclick_content STATIC
        src/content/ActorFiles.cpp
        src/content/ContentCheck.cpp
        src/content/DialogueFiles.cpp
        src/content/InteractionFiles.cpp
        src/content/RoomFiles.cpp
        src/content/RoomLibrary.cpp
        src/content/SaveFiles.cpp
        src/content/SpriteFields.cpp
        src/content/TextFormat.cpp)
target_link_libraries(pointclick_content PUBLIC pointclick_game)

# Devices -> abstract actions and viewport points; consumer chain.
add_library(pointclick_input STATIC
        src/input/InputSystem.cpp
        src/input/InputRouter.cpp)
target_link_libraries(pointclick_input PUBLIC pointclick_game)

# Gameplay UI consumers in viewport space.
add_library(pointclick_ui STATIC
        src/ui/DialogueMenu.cpp
        src/ui/SpeechLayout.cpp
        src/ui/TextLayout.cpp)
target_link_libraries(pointclick_ui PUBLIC pointclick_input)

# Read-only presentation.
add_library(pointclick_graphics STATIC
        src/graphics/EntityRenderer.cpp
        src/graphics/Renderer.cpp
        src/graphics/RoomBackdropRenderer.cpp
        src/graphics/SceneRenderer.cpp
        src/graphics/SpriteLibrary.cpp
        src/graphics/DialogueMenuRenderer.cpp
        src/graphics/FontLibrary.cpp
        src/graphics/SpeechRenderer.cpp
        src/graphics/TextRenderer.cpp)
target_link_libraries(pointclick_graphics PUBLIC pointclick_ui PRIVATE SDL3_ttf::SDL3_ttf)

set(POINTCLICK_LAYER_TARGETS pointclick_game pointclick_content pointclick_input
        pointclick_ui pointclick_graphics)
if(POINTCLICK_ENABLE_TOOLS)
    # Optional editor: another input consumer and another render overlay.
    add_library(pointclick_tools STATIC
            src/debug/DebugEditor.cpp
            src/debug/DebugOverlay.cpp
            src/debug/DiagnosticsOverlay.cpp)
    target_link_libraries(pointclick_tools PUBLIC pointclick_content pointclick_graphics)
    target_compile_definitions(pointclick_tools PUBLIC POINTCLICK_TOOLS=1)
    list(APPEND POINTCLICK_LAYER_TARGETS pointclick_tools)
endif()

# Command-line parsing without SDL, so tests can link it apart from Application.
add_library(pointclick_cli STATIC src/core/CommandLine.cpp)
target_link_libraries(pointclick_cli PUBLIC pointclick_game)

add_executable(pointclick src/main.cpp src/core/Application.cpp)
target_link_libraries(pointclick PRIVATE pointclick_cli)
if(POINTCLICK_ENABLE_TOOLS)
    target_link_libraries(pointclick PRIVATE pointclick_tools) # brings content + graphics
else()
    target_link_libraries(pointclick PRIVATE pointclick_content pointclick_graphics)
endif()

foreach(target ${POINTCLICK_LAYER_TARGETS} pointclick_cli pointclick)
    target_compile_options(${target} PRIVATE ${POINTCLICK_WARNINGS})
endforeach()

set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)
if(BUILD_TESTING)
    # Rooms built in C++ exist only here: the shipped binary never links them.
    add_library(pointclick_test_support STATIC tests/support/RoomPresets.cpp)
    target_include_directories(pointclick_test_support PUBLIC tests)
    target_link_libraries(pointclick_test_support PUBLIC pointclick_game)
    target_compile_options(pointclick_test_support PRIVATE ${POINTCLICK_WARNINGS})
    # GameTests links the domain and content only: gameplay runs without any renderer.
    add_executable(pointclick_tests tests/GameTests.cpp)
    target_link_libraries(pointclick_tests PRIVATE pointclick_content pointclick_test_support)
    add_executable(pointclick_sprite_tests tests/SpriteTests.cpp)
    target_link_libraries(pointclick_sprite_tests PRIVATE pointclick_content pointclick_graphics
            pointclick_test_support)
    target_compile_definitions(pointclick_sprite_tests PRIVATE
            POINTCLICK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    # Text layout links ui/ only: wrapping is tested with fake metrics, no window or GPU.
    add_executable(pointclick_text_tests tests/TextLayoutTests.cpp)
    target_link_libraries(pointclick_text_tests PRIVATE pointclick_ui)
    add_executable(pointclick_cli_tests tests/CommandLineTests.cpp)
    target_link_libraries(pointclick_cli_tests PRIVATE pointclick_cli pointclick_content)
    target_compile_definitions(pointclick_cli_tests PRIVATE
            POINTCLICK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    set(POINTCLICK_TEST_TARGETS pointclick_tests pointclick_sprite_tests pointclick_text_tests
            pointclick_cli_tests)
    if(POINTCLICK_ENABLE_TOOLS)
        add_executable(pointclick_editor_tests tests/EditorTests.cpp)
        target_link_libraries(pointclick_editor_tests PRIVATE pointclick_tools pointclick_test_support)
        target_compile_definitions(pointclick_editor_tests PRIVATE
                POINTCLICK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND POINTCLICK_TEST_TARGETS pointclick_editor_tests)
    endif()
    # The regression checks must also run in Release builds.
    foreach(test_target IN LISTS POINTCLICK_TEST_TARGETS)
        target_compile_options(${test_target} PRIVATE ${POINTCLICK_WARNINGS}
                ${POINTCLICK_KEEP_ASSERTS})
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
    # The real executable: --check exits 0 on the shipped rooms and non-zero on a broken folder.
    add_test(NAME pointclick_check_assets
            COMMAND pointclick --rooms ${CMAKE_CURRENT_SOURCE_DIR}/assets/rooms --check)
    add_test(NAME pointclick_check_broken_rooms
            COMMAND pointclick --rooms ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/broken_rooms --check)
    set_tests_properties(pointclick_check_broken_rooms PROPERTIES WILL_FAIL TRUE)
    add_test(NAME pointclick_layer_includes
            COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckLayerIncludes.cmake)
endif()

add_custom_command(TARGET pointclick POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:pointclick>/assets"
        COMMENT "Copying game assets")
