// Exercises 6.1 and 6.2: building new atomic operations from CAS.
#pragma once

#include <atomic>
#include <cstdint>

// 6.1: atomically set a = max(a, v); return the previous value.
// Bonus: don't write to `a` at all when v is not larger.
template <class T>
T fetch_max(std::atomic<T>& a, T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    // TODO: a load + store is NOT atomic. Use a compare_exchange loop.
    (void)order;
    T old = a.load();
    if (v > old) a.store(v);
    return old;
}

// 6.2: increment `count` only if the result stays <= limit; return whether
// it did. Must never let count exceed limit, even with many threads.
inline bool try_increment_below(std::atomic<std::uint32_t>& count, std::uint32_t limit) noexcept {
    // TODO: this check-then-act has a race.
    if (count.load() >= limit) return false;
    count.fetch_add(1);
    return true;
}
