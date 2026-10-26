// Exercise 9.1 (solution): a blocking MPMC queue with a spin-then-wait strategy.
//
// The fast path is MpmcBoundedQueue's try_push/try_pop. When it fails we
// spin briefly (cheap if the other side is about to act), then sleep in
// the kernel on a C++20 atomic wait. Two "epoch" counters act as futex
// words: producers bump `pushed_` after each push and notify; consumers bump
// `popped_` after each pop. We only call notify when someone may be waiting,
// which keeps the uncontended path free of system calls.
#pragma once

#include <atomic>
#include <cstdint>
#include <utility>

#include <llds/common/cpu.hpp>
#include <llds/queue/mpmc_bounded.hpp>

template <class T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : q_(capacity) {}

    void push(T v) {
        for (int spin = 0;; ++spin) {
            if (q_.try_push(std::move(v))) break;
            if (spin < kSpins) {
                llds::cpu_relax();
                continue;
            }
            // Read the epoch, re-check, then sleep until it changes.
            const auto e = popped_.load(std::memory_order_acquire);
            if (q_.try_push(std::move(v))) break;
            full_waiters_.fetch_add(1, std::memory_order_seq_cst);
            popped_.wait(e, std::memory_order_seq_cst);
            full_waiters_.fetch_sub(1, std::memory_order_relaxed);
        }
        pushed_.fetch_add(1, std::memory_order_seq_cst);
        if (empty_waiters_.load(std::memory_order_seq_cst)) pushed_.notify_all();
    }

    T pop() {
        T v;
        for (int spin = 0;; ++spin) {
            if (q_.try_pop(v)) break;
            if (spin < kSpins) {
                llds::cpu_relax();
                continue;
            }
            const auto e = pushed_.load(std::memory_order_acquire);
            if (q_.try_pop(v)) break;
            empty_waiters_.fetch_add(1, std::memory_order_seq_cst);
            pushed_.wait(e, std::memory_order_seq_cst);
            empty_waiters_.fetch_sub(1, std::memory_order_relaxed);
        }
        popped_.fetch_add(1, std::memory_order_seq_cst);
        if (full_waiters_.load(std::memory_order_seq_cst)) popped_.notify_all();
        return v;
    }

    bool try_pop(T& v) {
        if (!q_.try_pop(v)) return false;
        popped_.fetch_add(1, std::memory_order_seq_cst);
        if (full_waiters_.load(std::memory_order_seq_cst)) popped_.notify_all();
        return true;
    }

private:
    static constexpr int kSpins = 200;
    llds::MpmcBoundedQueue<T> q_;
    alignas(64) std::atomic<std::uint32_t> pushed_{0};
    alignas(64) std::atomic<std::uint32_t> popped_{0};
    alignas(64) std::atomic<std::uint32_t> empty_waiters_{0};
    alignas(64) std::atomic<std::uint32_t> full_waiters_{0};
};
