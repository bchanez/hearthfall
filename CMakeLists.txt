cmake_minimum_required(VERSION 3.24)

project(jeu
    VERSION 0.1.0
    DESCRIPTION "2D co-op fantasy ARPG — see GAME_DESIGN.md"
    LANGUAGES CXX)

# --- Global settings ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd / editor tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to a Debug build if the user didn't pick one (single-config gens).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Dependencies ------------------------------------------------------------
# SDL3 is expected to be found via the system (Homebrew: `brew install sdl3`).
# For a self-contained / cross-platform build later, swap this block for
# FetchContent or vcpkg — see README.
find_package(SDL3 REQUIRED CONFIG)

# Lua 5.4 for content scripting, used via its C API directly (no binding lib).
# Homebrew installs it keg-only under lua@5.4; point CMake there. (Lua 5.5+ is
# avoided only because we pin the data-file API to the stable 5.4 line.)
set(LUA54_ROOT "/opt/homebrew/opt/lua@5.4" CACHE PATH "lua@5.4 install prefix")
find_path(LUA_INCLUDE_DIR lua.h
    HINTS "${LUA54_ROOT}/include/lua5.4" "${LUA54_ROOT}/include/lua" "${LUA54_ROOT}/include")
find_library(LUA_LIBRARY NAMES lua5.4 lua-5.4 lua HINTS "${LUA54_ROOT}/lib")
if(NOT LUA_INCLUDE_DIR OR NOT LUA_LIBRARY)
    message(FATAL_ERROR "Lua 5.4 not found. Install it with:  brew install lua@5.4")
endif()

# --- Executable --------------------------------------------------------------
add_executable(jeu
    src/main.cpp
    src/Game.cpp
    src/Simulation.cpp
    src/Renderer.cpp
    src/ScriptEngine.cpp
    src/Net.cpp)

target_include_directories(jeu PRIVATE src third_party ${LUA_INCLUDE_DIR})

target_link_libraries(jeu PRIVATE SDL3::SDL3 ${LUA_LIBRARY})

# Warnings: strict, but not -Werror (keeps early prototyping friendly).
if(MSVC)
    target_compile_options(jeu PRIVATE /W4)
else()
    target_compile_options(jeu PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests -------------------------------------------------------------------
# Opt-in (ON by the 'debug' preset). Pulls GoogleTest via FetchContent, so the
# first configure with tests enabled needs network access.
option(JEU_BUILD_TESTS "Build unit tests" OFF)

if(JEU_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2)
    # Don't let GoogleTest override our compiler/linker settings on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(jeu_tests
        tests/vec2_test.cpp
        tests/inventory_test.cpp
        tests/item_test.cpp
        tests/playerstats_test.cpp
        tests/simulation_test.cpp
        tests/net_test.cpp
        tests/save_test.cpp
        src/Simulation.cpp
        src/Net.cpp
        src/ScriptEngine.cpp)
    target_include_directories(jeu_tests PRIVATE src third_party ${LUA_INCLUDE_DIR})
    target_link_libraries(jeu_tests PRIVATE GTest::gtest_main ${LUA_LIBRARY})

    include(GoogleTest)
    gtest_discover_tests(jeu_tests)
endif()
