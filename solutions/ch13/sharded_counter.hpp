// Exercise 13.1 (solution): a sharded (per-thread) counter.
//
// Increments go to a slot chosen by a per-thread index, so threads almost
// never share a cache line. read() sums the slots: it is not a snapshot (a
// concurrent add may or may not be included) but it is exact once the
// writers are quiescent, which is what statistics counters need.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include <llds/common/cacheline.hpp>

template <std::size_t Shards = 64>
class ShardedCounter {
    static_assert(llds::is_pow2(Shards));

public:
    void add(std::uint64_t n = 1) noexcept {
        // Several threads may map to the same shard, so this must be an RMW.
        shards_[my_shard()].value.fetch_add(n, std::memory_order_relaxed);
    }

    std::uint64_t read() const noexcept {
        std::uint64_t sum = 0;
        for (const auto& s : shards_) sum += s.value.load(std::memory_order_relaxed);
        return sum;
    }

private:
    static std::size_t my_shard() noexcept {
        static std::atomic<std::size_t> next{0};
        thread_local const std::size_t id = next.fetch_add(1, std::memory_order_relaxed) & (Shards - 1);
        return id;
    }

    struct alignas(llds::kFalseSharingRange) Shard {
        std::atomic<std::uint64_t> value{0};
    };
    std::array<Shard, Shards> shards_{};
};
