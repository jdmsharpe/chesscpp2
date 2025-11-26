#ifndef POLYGLOT_H
#define POLYGLOT_H

#include <cstdint>
#include <string>
#include <vector>

#include "Position.h"
#include "Types.h"

// Polyglot opening book support
// Format: 16 bytes per entry
//   - 8 bytes: Zobrist hash (big-endian)
//   - 2 bytes: Move (big-endian)
//   - 2 bytes: Weight (big-endian)
//   - 4 bytes: Learn data (ignored)

struct PolyglotEntry {
  uint64_t key;
  uint16_t move;
  uint16_t weight;
  uint32_t learn;
};

class PolyglotBook {
 public:
  PolyglotBook() = default;
  ~PolyglotBook() = default;

  // Load a .bin polyglot book file
  bool load(const std::string& filename);

  // Check if book is loaded
  bool isLoaded() const { return !entries.empty(); }

  // Get number of entries
  size_t size() const { return entries.size(); }

  // Probe the book for a position, returns 0 if no move found
  // Uses weighted random selection among book moves
  Move probe(const Position& pos) const;

  // Get all book moves for a position (for debugging/display)
  std::vector<std::pair<Move, uint16_t>> getMoves(const Position& pos) const;

  // Compute Polyglot-compatible hash for a position
  static uint64_t computeHash(const Position& pos);

 private:
  std::vector<PolyglotEntry> entries;

  // Binary search for entries with matching key
  std::pair<size_t, size_t> findEntries(uint64_t key) const;

  // Convert Polyglot move encoding to our Move format
  Move convertMove(uint16_t polyMove, const Position& pos) const;

  // Polyglot random numbers (standardized)
  static const uint64_t Random64[781];
  static const uint64_t RandomCastle[4];
  static const uint64_t RandomEnPassant[8];
  static const uint64_t RandomTurn;
};

#endif  // POLYGLOT_H
