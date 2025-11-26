cmake_minimum_required(VERSION 3.16)
project(chesscpp2 VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# Compiler flags for optimization
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)
endif()

# Find SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# Fathom library for Syzygy tablebase support
add_library(fathom STATIC
    lib/Fathom/src/tbprobe.c
)
target_include_directories(fathom PUBLIC lib/Fathom/src)
# Disable helper API since we use our own bitboard functions
target_compile_definitions(fathom PRIVATE TB_NO_HELPER_API)
# C99 standard for Fathom
set_target_properties(fathom PROPERTIES C_STANDARD 99)

# Source files
set(SOURCES
    src/AI.cpp
    src/Bitboard.cpp
    src/Game.cpp
    src/Magic.cpp
    src/MoveGen.cpp
    src/Polyglot.cpp
    src/Position.cpp
    src/Tablebase.cpp
    src/UCI.cpp
    src/Window.cpp
    src/Zobrist.cpp
    src/main.cpp
)

# Header files
set(HEADERS
    inc/AI.h
    inc/Bitboard.h
    inc/Game.h
    inc/Magic.h
    inc/MoveGen.h
    inc/Polyglot.h
    inc/Position.h
    inc/Tablebase.h
    inc/Types.h
    inc/UCI.h
    inc/Window.h
    inc/Zobrist.h
)

# Main executable
add_executable(chesscpp2 ${SOURCES} ${HEADERS})
target_include_directories(chesscpp2 PRIVATE inc ${SDL2_INCLUDE_DIRS} lib/Fathom/src)
target_link_libraries(chesscpp2 PRIVATE ${SDL2_LIBRARIES} SDL2_image stdc++ fathom)

# Tests
enable_testing()
add_subdirectory(test)
