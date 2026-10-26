// Exercise 2.1 (solution): per-thread counters without false sharing.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <llds/common/cacheline.hpp>

template <std::size_t N>
class PerThreadCounters {
public:
    // Called only by thread `slot`; relaxed is enough because nobody else
    // writes this counter and readers only need an eventually-correct sum.
    void add(std::size_t slot, std::uint64_t n) noexcept {
        auto& c = slots_[slot].value;
        c.store(c.load(std::memory_order_relaxed) + n, std::memory_order_relaxed);
    }

    std::uint64_t total() const noexcept {
        std::uint64_t sum = 0;
        for (const auto& s : slots_) sum += s.value.load(std::memory_order_relaxed);
        return sum;
    }

    const void* address_of(std::size_t slot) const noexcept { return &slots_[slot].value; }

private:
    // Each counter owns a full false-sharing range (128 bytes covers the
    // adjacent-line prefetcher on Intel and the 128-byte lines on Apple M).
    struct alignas(llds::kFalseSharingRange) Slot {
        std::atomic<std::uint64_t> value{0};
    };
    Slot slots_[N];
};
