#!/bin/bash
#
# Perft verification script - tests move generation against known values
#
# Usage: ./scripts/verify_perft.sh [path-to-engine]
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed

set -e

ENGINE="${1:-./build/chesscpp2}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

failed=0
passed=0

verify_perft() {
    local fen="$1"
    local depth="$2"
    local expected="$3"
    local name="$4"

    echo ""
    echo "Testing: $name (depth $depth)"

    if [ "$fen" = "startpos" ]; then
        output=$("$ENGINE" --perft "$depth" 2>&1)
    else
        output=$("$ENGINE" -f "$fen" --perft "$depth" 2>&1)
    fi

    actual=$(echo "$output" | grep "Total nodes:" | awk '{print $3}')

    if [ "$actual" = "$expected" ]; then
        echo -e "${GREEN}PASS${NC}: $actual nodes (expected $expected)"
        ((passed++))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $actual nodes (expected $expected)"
        ((failed++))
        return 1
    fi
}

echo "========================================"
echo "Chess++ Perft Verification Suite"
echo "========================================"
echo "Engine: $ENGINE"

# Starting position - well-known perft values
# Source: https://www.chessprogramming.org/Perft_Results
echo ""
echo "--- Starting Position ---"
verify_perft "startpos" 1 20 "Starting position" || true
verify_perft "startpos" 2 400 "Starting position" || true
verify_perft "startpos" 3 8902 "Starting position" || true
verify_perft "startpos" 4 197281 "Starting position" || true
verify_perft "startpos" 5 4865609 "Starting position" || true

# Kiwipete - complex position with many special moves
# Source: https://www.chessprogramming.org/Perft_Results#Position_2
echo ""
echo "--- Kiwipete (complex position) ---"
KIWIPETE="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
verify_perft "$KIWIPETE" 1 48 "Kiwipete" || true
verify_perft "$KIWIPETE" 2 2039 "Kiwipete" || true
verify_perft "$KIWIPETE" 3 97862 "Kiwipete" || true
verify_perft "$KIWIPETE" 4 4085603 "Kiwipete" || true

# Position 3 - tests en passant edge cases
# Source: https://www.chessprogramming.org/Perft_Results#Position_3
echo ""
echo "--- Position 3 (en passant) ---"
POS3="8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
verify_perft "$POS3" 1 14 "Position 3" || true
verify_perft "$POS3" 2 191 "Position 3" || true
verify_perft "$POS3" 3 2812 "Position 3" || true
verify_perft "$POS3" 4 43238 "Position 3" || true

# Position 4 - tests promotions
# Source: https://www.chessprogramming.org/Perft_Results#Position_4
echo ""
echo "--- Position 4 (promotions) ---"
POS4="r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
verify_perft "$POS4" 1 6 "Position 4" || true
verify_perft "$POS4" 2 264 "Position 4" || true
verify_perft "$POS4" 3 9467 "Position 4" || true
verify_perft "$POS4" 4 422333 "Position 4" || true

# Position 5 - another tricky position
# Source: https://www.chessprogramming.org/Perft_Results#Position_5
echo ""
echo "--- Position 5 ---"
POS5="rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
verify_perft "$POS5" 1 44 "Position 5" || true
verify_perft "$POS5" 2 1486 "Position 5" || true
verify_perft "$POS5" 3 62379 "Position 5" || true
verify_perft "$POS5" 4 2103487 "Position 5" || true

# Summary
echo ""
echo "========================================"
echo "SUMMARY"
echo "========================================"
echo -e "Passed: ${GREEN}$passed${NC}"
echo -e "Failed: ${RED}$failed${NC}"
echo "========================================"

if [ $failed -gt 0 ]; then
    echo -e "${RED}VERIFICATION FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}ALL TESTS PASSED${NC}"
    exit 0
fi
