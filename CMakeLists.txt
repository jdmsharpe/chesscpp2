cmake_minimum_required(VERSION 3.16)
project(chesscpp2 VERSION 1.0.0 LANGUAGES CXX)

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

# Source files
set(SOURCES
    src/main.cpp
    src/Bitboard.cpp
    src/Position.cpp
    src/MoveGen.cpp
    src/Magic.cpp
    src/Zobrist.cpp
    src/AI.cpp
    src/Game.cpp
    src/Window.cpp
)

# Header files
set(HEADERS
    inc/Bitboard.h
    inc/Position.h
    inc/MoveGen.h
    inc/Magic.h
    inc/Zobrist.h
    inc/AI.h
    inc/Game.h
    inc/Window.h
    inc/Types.h
)

# Main executable
add_executable(chesscpp2 ${SOURCES} ${HEADERS})
target_include_directories(chesscpp2 PRIVATE inc ${SDL2_INCLUDE_DIRS})
target_link_libraries(chesscpp2 PRIVATE ${SDL2_LIBRARIES} SDL2_image stdc++)

# Tests
enable_testing()
add_subdirectory(test)
