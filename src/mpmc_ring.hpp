#pragma once
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>
#include <vector>

namespace ull {

// Multi-producer multi-consumer bounded ring (capacity rounded up to power of two).
template <class T>
class MPMCRing {
  struct Cell {
    std::atomic<size_t> seq;
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
  };

  const size_t cap_;
  const size_t mask_;
  std::vector<Cell> buf_;
  alignas(64) std::atomic<size_t> head_{0};
  alignas(64) std::atomic<size_t> tail_{0};

  static size_t round_up_pow2(size_t x) {
    if (x < 2) return 2;
    --x;
    for (size_t i = 1; i < sizeof(size_t) * 8; i <<= 1) x |= (x >> i);
    return x + 1;
  }

public:
  explicit MPMCRing(size_t capacity)
      : cap_(round_up_pow2(capacity)),
        mask_(cap_ - 1),
        buf_(cap_) {
    for (size_t i = 0; i < cap_; ++i) buf_[i].seq.store(i, std::memory_order_relaxed);
  }

  MPMCRing(const MPMCRing&) = delete;
  MPMCRing& operator=(const MPMCRing&) = delete;

  template <class... A>
  bool emplace(A&&... a) {
    Cell* cell;
    size_t pos = head_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &buf_[pos & mask_];
      size_t s = cell->seq.load(std::memory_order_acquire);
      intptr_t diff = static_cast<intptr_t>(s) - static_cast<intptr_t>(pos);
      if (diff == 0) {
        if (head_.compare_exchange_weak(pos, pos + 1,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
          break;
        }
      } else if (diff < 0) {
        // full
        return false;
      } else {
        pos = head_.load(std::memory_order_relaxed);
      }
    }
    ::new (&cell->storage) T(std::forward<A>(a)...);
    cell->seq.store(pos + 1, std::memory_order_release);
    return true;
  }

  bool try_enqueue(const T& v) { return emplace(v); }
  bool try_enqueue(T&& v) { return emplace(std::move(v)); }

  bool try_dequeue(T& out) {
    Cell* cell;
    size_t pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &buf_[pos & mask_];
      size_t s = cell->seq.load(std::memory_order_acquire);
      intptr_t diff = static_cast<intptr_t>(s) - static_cast<intptr_t>(pos + 1);
      if (diff == 0) {
        if (tail_.compare_exchange_weak(pos, pos + 1,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
          break;
        }
      } else if (diff < 0) {
        // empty
        return false;
      } else {
        pos = tail_.load(std::memory_order_relaxed);
      }
    }
    T* ptr = reinterpret_cast<T*>(&cell->storage);
    out = std::move(*ptr);
    ptr->~T();
    cell->seq.store(pos + mask_ + 1, std::memory_order_release);
    return true;
  }

  bool empty() const noexcept {
    return head_.load(std::memory_order_relaxed) ==
           tail_.load(std::memory_order_relaxed);
  }

  size_t capacity() const noexcept { return cap_; }
};

} // namespace ull
