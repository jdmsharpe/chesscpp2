cmake_minimum_required(VERSION 3.16)
project(chesscpp2 VERSION 1.0.0 LANGUAGES CXX C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type if not specified (allows user override via -DCMAKE_BUILD_TYPE=...)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

option(CHESSCPP2_ENABLE_SANITIZERS "Enable ASan/UBSan for engine and tests" OFF)

# Compiler warnings (all build types)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(chesscpp2_sanitizers INTERFACE)
if(CHESSCPP2_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(chesscpp2_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(chesscpp2_sanitizers INTERFACE -fsanitize=address,undefined)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(chesscpp2_sanitizers INTERFACE stdc++ m)
        endif()
    else()
        message(WARNING "CHESSCPP2_ENABLE_SANITIZERS is only supported with GCC or Clang")
    endif()
endif()

# Find SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# Fathom library for Syzygy tablebase support
add_library(fathom STATIC
    lib/Fathom/src/tbprobe.c
)
target_include_directories(fathom PUBLIC lib/Fathom/src)
target_link_libraries(fathom PUBLIC chesscpp2_sanitizers)
# Disable helper API since we use our own bitboard functions
target_compile_definitions(fathom PRIVATE TB_NO_HELPER_API)
# C99 standard for Fathom
set_target_properties(fathom PROPERTIES C_STANDARD 99)

# Source files
set(SOURCES
    src/AI.cpp
    src/Bitboard.cpp
    src/Eval.cpp
    src/Game.cpp
    src/Magic.cpp
    src/MoveGen.cpp
    src/MovePicker.cpp
    src/Polyglot.cpp
    src/Position.cpp
    src/Tablebase.cpp
    src/UCI.cpp
    src/Window.cpp
    src/Zobrist.cpp
    src/main.cpp
)

# Main executable
add_executable(chesscpp2 ${SOURCES})
target_include_directories(chesscpp2 PRIVATE inc ${SDL2_INCLUDE_DIRS} lib/Fathom/src)
target_compile_features(chesscpp2 PRIVATE cxx_std_20)
set_target_properties(chesscpp2 PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(chesscpp2 PRIVATE ${SDL2_LIBRARIES} SDL2_image fathom chesscpp2_sanitizers)

# Performance flags for the engine (not applied to tests or debug builds)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT CHESSCPP2_ENABLE_SANITIZERS)
    target_compile_options(chesscpp2 PRIVATE -O3 -march=native)
endif()

# Tests
enable_testing()
add_subdirectory(test)
