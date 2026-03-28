#pragma once

#include "MoveGen.h"
#include "Polyglot.h"
#include "Position.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace testutil {

inline std::filesystem::path makeTempBookPath(std::string_view stem) {
  static std::atomic<uint64_t> counter{0};
  const auto uniqueId = counter.fetch_add(1, std::memory_order_relaxed);
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

  return std::filesystem::temp_directory_path() /
         (std::string(stem) + "_" + std::to_string(timestamp) + "_" + std::to_string(uniqueId) +
          ".bin");
}

class ScopedTempBookFile {
 public:
  explicit ScopedTempBookFile(std::string_view stem) : path_(makeTempBookPath(stem)) {}

  ~ScopedTempBookFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

inline void writeBigEndian(std::ofstream& out, uint64_t value, int byteCount) {
  for (int shift = (byteCount - 1) * 8; shift >= 0; shift -= 8) {
    out.put(static_cast<char>((value >> shift) & 0xFF));
  }
}

inline bool writePolyglotBook(const std::filesystem::path& path,
                              const std::vector<PolyglotEntry>& entries) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) return false;

  for (const auto& entry : entries) {
    writeBigEndian(out, entry.key, 8);
    writeBigEndian(out, entry.move, 2);
    writeBigEndian(out, entry.weight, 2);
    writeBigEndian(out, entry.learn, 4);
  }

  return out.good();
}

inline bool writeRawBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) return false;

  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  return out.good();
}

inline uint16_t encodePolyglotMove(std::string_view uci) {
  if (uci.size() < 4) return 0;

  const int fromFile = uci[0] - 'a';
  const int fromRank = uci[1] - '1';
  const int toFile = uci[2] - 'a';
  const int toRank = uci[3] - '1';

  int promo = 0;
  if (uci.size() == 5) {
    switch (uci[4]) {
      case 'n':
        promo = 1;
        break;
      case 'b':
        promo = 2;
        break;
      case 'r':
        promo = 3;
        break;
      case 'q':
        promo = 4;
        break;
      default:
        return 0;
    }
  }

  return static_cast<uint16_t>(toFile | (toRank << 3) | (fromFile << 6) | (fromRank << 9) |
                               (promo << 12));
}

inline Move findLegalMove(const Position& pos, std::string_view uci) {
  if (uci.size() < 4) return 0;

  const int fromFile = uci[0] - 'a';
  const int fromRank = uci[1] - '1';
  const int toFile = uci[2] - 'a';
  const int toRank = uci[3] - '1';

  if (fromFile < FILE_A || fromFile > FILE_H || fromRank < RANK_1 || fromRank > RANK_8 ||
      toFile < FILE_A || toFile > FILE_H || toRank < RANK_1 || toRank > RANK_8) {
    return 0;
  }

  const Square from = makeSquare(fromFile, fromRank);
  const Square to = makeSquare(toFile, toRank);

  PieceType promoType = NO_PIECE_TYPE;
  if (uci.size() == 5) {
    switch (uci[4]) {
      case 'n':
        promoType = KNIGHT;
        break;
      case 'b':
        promoType = BISHOP;
        break;
      case 'r':
        promoType = ROOK;
        break;
      case 'q':
        promoType = QUEEN;
        break;
      default:
        return 0;
    }
  }

  Position posCopy = pos;
  MoveList legalMoves = MoveGen::generateLegalMoves(posCopy);
  for (Move move : legalMoves) {
    if (fromSquare(move) != from || toSquare(move) != to) continue;
    if (uci.size() == 5) {
      if (moveType(move) == PROMOTION && promotionType(move) == promoType) {
        return move;
      }
      continue;
    }
    if (moveType(move) != PROMOTION) return move;
  }

  return 0;
}

}  // namespace testutil
