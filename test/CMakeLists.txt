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

# Shared static libraries to avoid recompiling the same sources per test target
# "chess_core" — board, movegen, bitboards (no AI/eval dependency)
add_library(chess_core STATIC
    ../src/Bitboard.cpp
    ../src/Magic.cpp
    ../src/Zobrist.cpp
    ../src/Position.cpp
    ../src/MoveGen.cpp
)
target_include_directories(chess_core PUBLIC ../inc)
target_compile_features(chess_core PUBLIC cxx_std_20)
set_target_properties(chess_core PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(chess_core PUBLIC chesscpp2_sanitizers)

# "chess_engine" — full engine (core + eval + AI + books + tablebases)
add_library(chess_engine STATIC
    ../src/AI.cpp
    ../src/Eval.cpp
    ../src/Game.cpp
    ../src/MovePicker.cpp
    ../src/UCI.cpp
    ../src/Polyglot.cpp
    ../src/Tablebase.cpp
)
target_include_directories(chess_engine PUBLIC ../inc ../lib/Fathom/src)
target_link_libraries(chess_engine PUBLIC chess_core fathom)
target_link_libraries(chess_engine PUBLIC chesscpp2_sanitizers)

function(chesscpp2_enable_test_instrumentation target)
    target_link_libraries(${target} PRIVATE chesscpp2_sanitizers)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${target} PRIVATE m)
    endif()
endfunction()

# --- Test targets ---

# Bitboard tests
add_executable(test_bitboard test_bitboard.cpp)
target_link_libraries(test_bitboard PRIVATE chess_core GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_bitboard)

# MoveGen tests
add_executable(test_movegen test_movegen.cpp)
target_link_libraries(test_movegen PRIVATE chess_core GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_movegen)

# Position tests (draw detection, make/unmake, SEE)
add_executable(test_position test_position.cpp)
target_link_libraries(test_position PRIVATE chess_core GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_position)

# Perft benchmark suite (standalone tool, not a test)
add_executable(perft_suite perft_suite.cpp)
target_link_libraries(perft_suite PRIVATE chess_core pthread)
chesscpp2_enable_test_instrumentation(perft_suite)

# Polyglot tests
add_executable(test_polyglot test_polyglot.cpp ../src/Polyglot.cpp)
target_link_libraries(test_polyglot PRIVATE chess_core GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_polyglot)

# Eval tests
add_executable(test_eval test_eval.cpp ../src/Eval.cpp)
target_link_libraries(test_eval PRIVATE chess_core GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_eval)

# AI tests
add_executable(test_ai test_ai.cpp)
target_link_libraries(test_ai PRIVATE chess_engine GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_ai)

# UCI tests
add_executable(test_uci test_uci.cpp)
target_link_libraries(test_uci PRIVATE chess_engine GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_uci)

# Game tests
add_executable(test_game test_game.cpp)
target_link_libraries(test_game PRIVATE chess_engine GTest::gtest_main pthread)
chesscpp2_enable_test_instrumentation(test_game)

# Discover tests
include(GoogleTest)
gtest_discover_tests(test_bitboard)
gtest_discover_tests(test_movegen)
gtest_discover_tests(test_position)
gtest_discover_tests(test_polyglot)
gtest_discover_tests(test_eval)
gtest_discover_tests(test_ai)
gtest_discover_tests(test_uci)
gtest_discover_tests(test_game)
