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

# Position tests (draw detection, make/unmake, SEE)
add_executable(test_position
    test_position.cpp
    ../src/Bitboard.cpp
    ../src/Magic.cpp
    ../src/Zobrist.cpp
    ../src/Position.cpp
    ../src/MoveGen.cpp
)
target_include_directories(test_position PRIVATE ../inc)
target_link_libraries(test_position PRIVATE GTest::gtest_main pthread)

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

# Polyglot tests
add_executable(test_polyglot
    test_polyglot.cpp
    ../src/Bitboard.cpp
    ../src/Magic.cpp
    ../src/Zobrist.cpp
    ../src/Position.cpp
    ../src/MoveGen.cpp
    ../src/Polyglot.cpp
)
target_include_directories(test_polyglot PRIVATE ../inc)
target_link_libraries(test_polyglot PRIVATE GTest::gtest_main pthread)

# Eval tests
add_executable(test_eval
    test_eval.cpp
    ../src/Eval.cpp
    ../src/Bitboard.cpp
    ../src/Magic.cpp
    ../src/Zobrist.cpp
    ../src/Position.cpp
    ../src/MoveGen.cpp
)
target_include_directories(test_eval PRIVATE ../inc)
target_link_libraries(test_eval PRIVATE GTest::gtest_main pthread)

# AI tests
add_executable(test_ai
    test_ai.cpp
    ../src/AI.cpp
    ../src/Eval.cpp
    ../src/Bitboard.cpp
    ../src/Magic.cpp
    ../src/Zobrist.cpp
    ../src/Position.cpp
    ../src/MoveGen.cpp
    ../src/Polyglot.cpp
    ../src/Tablebase.cpp
)
target_include_directories(test_ai PRIVATE ../inc ../lib/Fathom/src)
target_link_libraries(test_ai PRIVATE GTest::gtest_main fathom pthread)

# Discover tests
include(GoogleTest)
gtest_discover_tests(test_bitboard)
gtest_discover_tests(test_movegen)
gtest_discover_tests(test_position)
gtest_discover_tests(test_polyglot)
gtest_discover_tests(test_eval)
gtest_discover_tests(test_ai)
