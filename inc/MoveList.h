#pragma once

#include <array>
#include <cstddef>

// Maximum number of legal moves in any chess position is 218 (theoretical max).
// We use 256 for alignment and headroom.
inline constexpr size_t MAX_MOVES = 256;

// Stack-allocated fixed-capacity list. Zero heap allocation.
// Drop-in replacement for std::vector<T> in move generation hot paths.
template <typename T, size_t N = MAX_MOVES>
class FixedList {
 public:
  using iterator = T*;
  using const_iterator = const T*;

  FixedList() = default;

  void push_back(const T& v) { data_[size_++] = v; }

  [[nodiscard]] size_t size() const { return size_; }
  [[nodiscard]] bool empty() const { return size_ == 0; }

  T& operator[](size_t i) { return data_[i]; }
  const T& operator[](size_t i) const { return data_[i]; }

  T* begin() { return data_.data(); }
  T* end() { return data_.data() + size_; }
  const T* begin() const { return data_.data(); }
  const T* end() const { return data_.data() + size_; }

  T* data() { return data_.data(); }
  const T* data() const { return data_.data(); }

  void clear() { size_ = 0; }

  // Append a range [first, last)
  void append(const T* first, const T* last) {
    while (first != last) {
      data_[size_++] = *first++;
    }
  }

  // Remove element at index by swapping with last (O(1), unordered)
  void swapErase(size_t i) { data_[i] = data_[--size_]; }

 private:
  std::array<T, N> data_;
  size_t size_ = 0;
};
