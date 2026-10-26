// Exercise 7.1 (solution): a seqlock written from scratch.
#pragma once

#include <atomic>
#include <cstdint>

#include <llds/common/cpu.hpp>

struct Vitals4 {
    std::uint64_t heart_rate, spo2, resp_rate, timestamp;
};

class VitalsBoard {
public:
    // Single writer.
    void publish(const Vitals4& v) noexcept {
        const auto s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);         // odd = writing
        std::atomic_thread_fence(std::memory_order_release);  // seq store before data stores
        hr_.store(v.heart_rate, std::memory_order_relaxed);
        spo2_.store(v.spo2, std::memory_order_relaxed);
        rr_.store(v.resp_rate, std::memory_order_relaxed);
        ts_.store(v.timestamp, std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);         // data stores before seq store
    }

    // Any number of readers.
    Vitals4 read() const noexcept {
        for (;;) {
            const auto s1 = seq_.load(std::memory_order_acquire);  // data loads after this
            if (s1 & 1) {
                llds::cpu_relax();
                continue;
            }
            Vitals4 v{hr_.load(std::memory_order_relaxed), spo2_.load(std::memory_order_relaxed),
                      rr_.load(std::memory_order_relaxed), ts_.load(std::memory_order_relaxed)};
            std::atomic_thread_fence(std::memory_order_acquire);  // data loads before s2
            if (seq_.load(std::memory_order_relaxed) == s1) return v;
        }
    }

private:
    std::atomic<std::uint64_t> seq_{0};
    std::atomic<std::uint64_t> hr_{0}, spo2_{0}, rr_{0}, ts_{0};
};
