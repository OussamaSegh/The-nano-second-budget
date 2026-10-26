// Exercise 16.1 (solution): the SPSC ring with both bugs fixed.
//
// Bug 1 (capacity): the full check compared with `>` instead of `==`, so
//        the producer could write one element past capacity, overwriting
//        the oldest unread slot.
// Bug 2 (ordering): tail_ was published with memory_order_relaxed. The
//        element write could become visible after the index, so the
//        consumer could read a stale slot. Release on the producer's tail
//        store, acquire on the consumer's tail load (and the same pair for
//        head_ in the other direction).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

template <class T>
class SpscRingExercise {
public:
    explicit SpscRingExercise(std::size_t capacity) : cap_(capacity), buf_(capacity) {}

    bool try_push(const T& v) {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t - head_.load(std::memory_order_acquire) == cap_) return false;  // fixed: ==
        buf_[t % cap_] = v;
        tail_.store(t + 1, std::memory_order_release);                         // fixed: release
        return true;
    }

    bool try_pop(T& out) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;          // fixed: acquire
        out = buf_[h % cap_];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

private:
    const std::uint64_t cap_;
    std::vector<T> buf_;
    alignas(64) std::atomic<std::uint64_t> head_{0};
    alignas(64) std::atomic<std::uint64_t> tail_{0};
};
