cmake_minimum_required(VERSION 3.26)

project(
        pointclick
        VERSION 0.1.0
        LANGUAGES CXX
)

include(cmake/CPM.cmake)
include(cmake/Dependencies.cmake)

add_executable(pointclick)

target_sources(
        pointclick
        PRIVATE
        src/main.cpp
        src/core/Application.cpp
        src/core/Application.hpp
        src/game/FloatingText.cpp
        src/game/FloatingText.hpp
        src/game/Game.cpp
        src/game/Game.hpp
        src/game/Navigation.cpp
        src/game/Navigation.hpp
        src/game/Player.cpp
        src/game/Player.hpp
        src/game/Room.cpp
        src/game/Room.hpp
        src/game/SceneObject.hpp
        src/game/World.hpp
        src/graphics/Renderer.cpp
        src/graphics/Renderer.hpp
)

target_compile_features(pointclick PRIVATE cxx_std_20)
target_include_directories(pointclick PRIVATE src)

target_link_libraries(
        pointclick
        PRIVATE
        SDL3::SDL3
)

include(CTest)
if(BUILD_TESTING)
    add_executable(pointclick_tests
            tests/GameTests.cpp
            src/game/FloatingText.cpp
            src/game/Game.cpp
            src/game/Navigation.cpp
            src/game/Player.cpp)
    target_sources(pointclick_tests PRIVATE src/game/Room.cpp)
    target_compile_features(pointclick_tests PRIVATE cxx_std_20)
    target_include_directories(pointclick_tests PRIVATE src)
    target_link_libraries(pointclick_tests PRIVATE SDL3::SDL3)
    add_test(NAME pointclick_tests COMMAND pointclick_tests)
endif()

if(MSVC)
    target_compile_options(pointclick PRIVATE /W4 /permissive-)
else()
    target_compile_options(pointclick PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(
        TARGET pointclick POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:pointclick>/assets"
        COMMENT "Copying game assets things"
)
