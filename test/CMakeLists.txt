# Find Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Override compiler to avoid distcc issues with tests
set(CMAKE_CXX_COMPILER_LAUNCHER "")

# Bitboard tests
add_executable(test_bitboard test_bitboard.cpp ../src/Bitboard.cpp)
target_include_directories(test_bitboard PRIVATE ../inc)
target_link_libraries(test_bitboard PRIVATE GTest::gtest_main pthread)

# MoveGen tests
add_executable(test_movegen
    test_movegen.cpp
    ../src/Bitboard.cpp
    ../src/Magic.cpp
    ../src/Zobrist.cpp
    ../src/Position.cpp
    ../src/MoveGen.cpp
)
target_include_directories(test_movegen PRIVATE ../inc)
target_link_libraries(test_movegen PRIVATE GTest::gtest_main pthread)

# Perft benchmark suite (standalone tool, not a test)
add_executable(perft_suite
    perft_suite.cpp
    ../src/Bitboard.cpp
    ../src/Magic.cpp
    ../src/Zobrist.cpp
    ../src/Position.cpp
    ../src/MoveGen.cpp
)
target_include_directories(perft_suite PRIVATE ../inc)
target_link_libraries(perft_suite PRIVATE pthread)

# Discover tests
include(GoogleTest)
gtest_discover_tests(test_bitboard)
gtest_discover_tests(test_movegen)
