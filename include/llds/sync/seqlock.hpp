// llds/sync/seqlock.hpp
// A single-writer sequence lock (Chapter 7).
//
// Readers never write shared memory, so any number of them can read without
// slowing the writer or each other. A reader that overlaps a write detects it
// through the sequence counter and retries.
//
// The payload is stored as an array of relaxed atomic words rather than as a
// plain T. A naive seqlock that memcpy's a plain struct while the writer is
// modifying it has a data race, which is undefined behaviour in C++ even
// though the torn copy is later discarded. Relaxed atomics make the racy
// copy well-defined at (on x86 and ARM) zero extra cost. See Boehm,
// "Can Seqlocks Get Along With Programming Language Memory Models?" (2012).
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../common/cacheline.hpp"
#include "../common/cpu.hpp"

namespace llds {

template <class T>
class SeqLock {
    static_assert(std::is_trivially_copyable_v<T>, "SeqLock payload must be trivially copyable");
    static constexpr std::size_t kWords = (sizeof(T) + 7) / 8;

public:
    SeqLock() = default;
    explicit SeqLock(const T& init) { store(init); }

    // Single writer only. Use a mutex around store() if you have several.
    void store(const T& value) noexcept {
        std::array<std::uint64_t, kWords> buf{};
        std::memcpy(buf.data(), &value, sizeof(T));
        const auto s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);         // odd: write in progress
        std::atomic_thread_fence(std::memory_order_release);  // keep data stores after it
        for (std::size_t i = 0; i < kWords; ++i)
            data_[i].store(buf[i], std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);         // even: done
    }

    // The writer never waits. Readers are blocking, not lock-free: a reader
    // retries while a write overlaps it, so it spins for as long as a writer
    // that stalls mid-write stays stalled. Use try_load with a retry budget
    // where that matters.
    T load() const noexcept {
        std::array<std::uint64_t, kWords> buf;
        for (;;) {
            const auto s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1) {  // writer active
                cpu_relax();
                continue;
            }
            for (std::size_t i = 0; i < kWords; ++i)
                buf[i] = data_[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);  // keep data loads before s2
            const auto s2 = seq_.load(std::memory_order_relaxed);
            if (s1 == s2) break;
        }
        T out;
        std::memcpy(static_cast<void*>(&out), buf.data(), sizeof(T));
        return out;
    }

    // Returns false instead of retrying; useful for readers with a deadline.
    bool try_load(T& out) const noexcept {
        std::array<std::uint64_t, kWords> buf;
        const auto s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1) return false;
        for (std::size_t i = 0; i < kWords; ++i) buf[i] = data_[i].load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (seq_.load(std::memory_order_relaxed) != s1) return false;
        std::memcpy(static_cast<void*>(&out), buf.data(), sizeof(T));
        return true;
    }

    std::uint64_t version() const noexcept { return seq_.load(std::memory_order_acquire) / 2; }

private:
    alignas(kCacheLine) std::atomic<std::uint64_t> seq_{0};
    std::array<std::atomic<std::uint64_t>, kWords> data_{};
};

}  // namespace llds
