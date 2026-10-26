// Exercise 7.1: write a seqlock from scratch (without llds::SeqLock).
//
// One writer thread calls publish(); many readers call read(). Readers must
// never return a mix of two different publishes (a "torn" read), and must
// never block the writer. The fields are relaxed atomics, so the program is
// race-free; your job is the sequence counter and the fences.
#pragma once

#include <atomic>
#include <cstdint>

#include <llds/common/cpu.hpp>

struct Vitals4 {
    std::uint64_t heart_rate, spo2, resp_rate, timestamp;
};

class VitalsBoard {
public:
    void publish(const Vitals4& v) noexcept {
        // TODO: without a sequence counter, readers can see half an update.
        hr_.store(v.heart_rate, std::memory_order_relaxed);
        spo2_.store(v.spo2, std::memory_order_relaxed);
        rr_.store(v.resp_rate, std::memory_order_relaxed);
        ts_.store(v.timestamp, std::memory_order_relaxed);
    }

    Vitals4 read() const noexcept {
        // TODO: detect overlapping writes and retry.
        return Vitals4{hr_.load(std::memory_order_relaxed), spo2_.load(std::memory_order_relaxed),
                       rr_.load(std::memory_order_relaxed), ts_.load(std::memory_order_relaxed)};
    }

private:
    std::atomic<std::uint64_t> seq_{0};
    std::atomic<std::uint64_t> hr_{0}, spo2_{0}, rr_{0}, ts_{0};
};
