// Exercise 2.1: per-thread counters without false sharing.
//
// Each of N threads increments only its own counter; total() sums them.
// The starter version below is functionally correct but every counter
// shares cache lines with its neighbours, so it is slow under contention
// and fails the layout test.
//
// Task: change the layout so each counter owns its own cache line (better:
// a full 128-byte false-sharing range). Keep add() free of atomic
// read-modify-write instructions: only one thread writes each counter.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <llds/common/cacheline.hpp>

template <std::size_t N>
class PerThreadCounters {
public:
    void add(std::size_t slot, std::uint64_t n) noexcept {
        // TODO: this is an atomic RMW (lock xadd on x86). Is it needed?
        counters_[slot].fetch_add(n, std::memory_order_relaxed);
    }

    std::uint64_t total() const noexcept {
        std::uint64_t sum = 0;
        for (const auto& c : counters_) sum += c.load(std::memory_order_relaxed);
        return sum;
    }

    const void* address_of(std::size_t slot) const noexcept { return &counters_[slot]; }

private:
    // TODO: 8 counters share one 64-byte line here.
    std::atomic<std::uint64_t> counters_[N]{};
};
