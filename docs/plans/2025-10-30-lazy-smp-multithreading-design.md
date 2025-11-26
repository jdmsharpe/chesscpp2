# Lazy SMP Multithreading Design

**Date:** 2025-10-30
**Goal:** Implement Lazy SMP parallelization with 4 threads to achieve 2.5-3x search speedup
**Approach:** Shared transposition table with independent thread-local search heuristics

---

## Overview

Lazy SMP (Shared Memory Parallelization) launches multiple search threads that run the same negamax algorithm independently but share a common transposition table. The "lazy" aspect means there's no complex work-stealing or coordination - threads naturally help each other through TT sharing.

**Expected Performance:**

- 4 threads: 2.5-3.0x speedup
- Depth 6 search: 60s → 20s
- Scalable to 8+ threads in future

---

## Architecture

### Thread Model

- **Main thread (Thread 0):** Manages iterative deepening, coordinates workers, reports results
- **Worker threads (1-3):** Run search independently with variations
- **Shared resources:** Transposition table, node counters
- **Thread-local resources:** Killer moves, history table, countermoves, PV table

### Thread Lifecycle

```text
Main Thread                    Worker Threads (1-3)
    |                                  |
    |-- Create thread pool             |
    |-- Start iterative deepening      |
    |                                   |
    |-- Signal: "Search depth D"  ---->|-- Start negamax(depth=D)
    |-- Start negamax(depth=D)          |-- Search assigned moves
    |-- Search assigned moves           |-- Write to shared TT
    |                                   |
    |-- Wait for all to complete  <----|-- Signal: "Done"
    |-- Collect best move/score         |
    |-- Next iteration or stop     ---->|
```

### Root Move Distribution

Moves distributed using **interleaved** pattern for load balancing:

```text
Given 20 legal moves:
Thread 0: [0, 4, 8, 12, 16]
Thread 1: [1, 5, 9, 13, 17]
Thread 2: [2, 6, 10, 14, 18]
Thread 3: [3, 7, 11, 15, 19]
```

**Why interleaved?** Moves are ordered best-first. Interleaving ensures each thread gets a mix of good/bad moves, preventing load imbalance.

---

## Thread-Safe Transposition Table

### The Challenge

Multiple threads reading/writing simultaneously causes race conditions:

- Corrupted data from partial reads/writes
- Good cutoffs overwritten by shallow searches
- Illegal moves from position corruption

### Solution: Lockless with Atomic Operations

**Modified TTEntry Structure:**

```cpp
struct TTEntry {
    std::atomic<HashKey> key;  // 64-bit atomic
    int16_t depth;
    int16_t score;
    Move bestMove;             // 16-bit
    uint8_t flag;
    uint8_t age;
    uint16_t padding;          // Align to 16 bytes
};
```

**Thread-Safe Access Pattern:**

**Probe (read):**

1. Atomically read `key`
2. If key matches hash, read other fields (safe because key validates data)
3. If key doesn't match, entry is invalid

**Store (write):**

1. Check replacement criteria (don't overwrite better entries)
2. Write depth, score, bestMove, flag, age
3. Memory fence
4. Atomically write key (makes entry valid)

**Why this works:**

- Invalid key = entry not yet valid or being written, skip it
- Valid key = all fields are consistent (written before key)
- Worst case: miss a TT hit (safe, just slower)
- No locks needed, no contention

---

## Thread Variation & Search Diversity

Threads must search differently to avoid redundant work. Each thread uses variations:

### 1. Aspiration Window Variation

```text
Thread 0: Standard (±50 centipawns)
Thread 1: Wider (±75 cp) - more stable
Thread 2: Narrower (±35 cp) - aggressive pruning
Thread 3: Full window every 3rd iteration
```

### 2. Late Move Reduction (LMR) Variation

```text
Thread 0: Standard reduction
Thread 1: +1 reduction (faster, shallower)
Thread 2: -1 reduction (slower, deeper)
Thread 3: Randomized ±1
```

### 3. Move Ordering Randomization

After first 3-4 moves, add small random perturbation (±50 cp) to move scores for diversity.

### 4. Null Move Reduction Variation

```text
Thread 0: R = 3 (standard)
Thread 1: R = 4 (aggressive)
Thread 2: R = 2 (conservative)
Thread 3: Skip null move every 4th node
```

**Result:** Threads explore different parts of the search tree while TT sharing propagates the best discoveries to all threads.

---

## Data Structure Organization

### Shared Resources (Thread-Safe Access)

```cpp
// Shared transposition table
std::vector<TTEntry> transpositionTable;  // Atomic operations

// Shared counters
std::atomic<uint64_t> nodesSearched;
std::atomic<uint64_t> ttHits;
std::atomic<bool> stopSearch;
```

### Thread-Local Data (No Sharing)

```cpp
struct ThreadData {
    int threadId;

    // Search heuristics (thread-specific)
    std::array<std::array<Move, 2>, 64> killerMoves;
    std::array<std::array<int, 64>, 64> historyTable;
    std::array<std::array<Move, 64>, 64> countermoves;
    std::array<std::array<Move, 64>, 64> pvTable;
    std::array<int, 64> pvLength;

    // Position copy (independent make/unmake)
    Position pos;

    // Results
    Move bestMove;
    int bestScore;
    std::atomic<bool> completed;
};

std::vector<ThreadData> threadData;  // One per thread
```

**Why this separation?**

- TT shared = massive benefit from cutoff sharing
- Killers/history separate = no lock contention, unique thread personalities
- Position separate = no race conditions on board state

**Memory cost:** ~1.4 MB for 4 threads (350 KB each) - negligible

---

## Testing & Validation

### Correctness Tests

1. **Perft verification:** Multithreaded perft must match single-threaded node counts
2. **Move legality:** All returned moves must be legal
3. **Determinism:** Same position should give consistent results (±timing variation)
4. **TT integrity:** No crashes from race conditions

### Performance Benchmarks

| Depth | 1 Thread | 4 Threads | Speedup |
|-------|----------|-----------|---------|
| 4     | 1.0s     | 0.4s      | 2.5x    |
| 5     | 10s      | 3.5s      | 2.8x    |
| 6     | 60s      | 20s       | 3.0x    |
| 7     | 300s     | 100s      | 3.0x    |

### Success Criteria

✓ No crashes after 1000+ searches
✓ All moves legal
✓ 2.5x+ speedup on 4 cores at depth 5+
✓ Perft counts match single-threaded
✓ Playing strength improves (deeper searches in same time)

---

## Expected Outcomes

**Performance Gains:**

- 2.5-3x faster search with 4 threads
- Depth 6 becomes practical for real-time play (~20s)
- Depth 7 becomes reachable in tournament time controls

**Code Impact:**

- ~300-400 lines added
- ~100-150 lines modified
- Total: ~500 lines across 3 files

**Scalability:**

- Architecture supports 8+ threads with minimal changes
- Just increase thread count and adjust variations

---

## References

- Lazy SMP technique used by Stockfish, Komodo
- Transposition table atomics based on modern chess engine practices
- Thread variation strategies from parallel alpha-beta research
