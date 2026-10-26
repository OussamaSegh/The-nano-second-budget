// Exercise 16.1: find the bugs.
//
// This SPSC ring has exactly two bugs. One is a plain logic error that a
// careful single-threaded test will find. The other is a memory-ordering
// bug that may never show up as a wrong answer on x86, but that
// ThreadSanitizer reports immediately:
//
//   cmake -S . -B build-tsan -DLLDS_SANITIZE=thread
//   cmake --build build-tsan --target ex_ch16_spsc_find_the_bugs
//   ./build-tsan/ex_ch16_spsc_find_the_bugs
//
// Find and fix both. For each, write down the test that caught it.
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
        if (t - head_.load(std::memory_order_acquire) > cap_) return false;
        buf_[t % cap_] = v;
        tail_.store(t + 1, std::memory_order_relaxed);
        return true;
    }

    bool try_pop(T& out) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_relaxed)) return false;
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
