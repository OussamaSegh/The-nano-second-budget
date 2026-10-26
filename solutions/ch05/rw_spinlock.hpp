// Exercise 5.1 (solution): a writer-preferring reader-writer spinlock.
//
// State word: bit 0 = writer holds the lock, bit 1 = a writer is waiting,
// bits 2.. = reader count. A waiting writer blocks *new* readers, so a
// steady stream of readers cannot starve writers.
#pragma once

#include <atomic>
#include <cstdint>

#include <llds/common/cpu.hpp>

class RwSpinLock {
    static constexpr std::uint32_t kWriter = 1;
    static constexpr std::uint32_t kWaiting = 2;
    static constexpr std::uint32_t kReader = 4;

public:
    void lock() noexcept {
        for (;;) {
            auto s = state_.load(std::memory_order_relaxed);
            // Free (possibly with our own or another's waiting bit): take it.
            if ((s & ~kWaiting) == 0) {
                if (state_.compare_exchange_weak(s, kWriter, std::memory_order_acquire,
                                                 std::memory_order_relaxed))
                    return;
                continue;
            }
            // Otherwise announce that a writer is waiting, then spin.
            if (!(s & kWaiting)) state_.fetch_or(kWaiting, std::memory_order_relaxed);
            llds::cpu_relax();
        }
    }

    bool try_lock() noexcept {
        std::uint32_t expected = 0;
        return state_.compare_exchange_strong(expected, kWriter, std::memory_order_acquire,
                                              std::memory_order_relaxed);
    }

    void unlock() noexcept { state_.fetch_and(~kWriter, std::memory_order_release); }

    void lock_shared() noexcept {
        for (;;) {
            if (try_lock_shared()) return;
            llds::cpu_relax();
        }
    }

    bool try_lock_shared() noexcept {
        auto s = state_.load(std::memory_order_relaxed);
        while (!(s & (kWriter | kWaiting))) {
            if (state_.compare_exchange_weak(s, s + kReader, std::memory_order_acquire,
                                             std::memory_order_relaxed))
                return true;
        }
        return false;
    }

    void unlock_shared() noexcept { state_.fetch_sub(kReader, std::memory_order_release); }

private:
    std::atomic<std::uint32_t> state_{0};
};
