// Exercise 13.1: a sharded counter.
//
// The starter is a single atomic: correct, but every add() from every
// thread fights over one cache line. Spread the count over `Shards` padded
// slots, choose each thread's slot once (thread_local), and make read()
// sum them. Think about whether add() still needs an atomic RMW.
//
// The test checks correctness and that shards really are separate lines.
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
    void add(std::uint64_t n = 1) noexcept { total_.fetch_add(n, std::memory_order_relaxed); }
    std::uint64_t read() const noexcept { return total_.load(std::memory_order_relaxed); }

private:
    std::atomic<std::uint64_t> total_{0};  // TODO: shard me
};
