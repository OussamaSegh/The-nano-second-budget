// Exercises 6.1 and 6.2 (solutions): building new atomic operations from CAS.
#pragma once

#include <atomic>
#include <cstdint>

// 6.1: atomically set a = max(a, v); return the previous value.
// (C++26 adds std::atomic<T>::fetch_max; this is how it is implemented
// on hardware without a native instruction.)
template <class T>
T fetch_max(std::atomic<T>& a, T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    T cur = a.load(std::memory_order_relaxed);
    // Exit early without writing if we would not change anything: a CAS
    // that writes the same value still takes the line in exclusive state.
    while (cur < v && !a.compare_exchange_weak(cur, v, order, std::memory_order_relaxed)) {
    }
    return cur;
}

// 6.2: increment `count` only if the result stays <= limit. Returns true on
// success. This is the admission check of a bounded resource (a counting
// semaphore without blocking, a connection limit, a token bucket).
inline bool try_increment_below(std::atomic<std::uint32_t>& count, std::uint32_t limit) noexcept {
    std::uint32_t cur = count.load(std::memory_order_relaxed);
    do {
        if (cur >= limit) return false;
    } while (!count.compare_exchange_weak(cur, cur + 1, std::memory_order_acquire,
                                          std::memory_order_relaxed));
    return true;
}
