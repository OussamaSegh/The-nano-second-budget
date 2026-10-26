// Exercise 8.1: an SPSC ring with batch operations.
//
// Implement push_batch and pop_batch so that a batch of k items costs ONE
// release store of the index, and so that each side reads the other side's
// index only when its cached copy says there is not enough room / data.
// Partial batches are allowed: return how many items were transferred.
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include <llds/common/cacheline.hpp>

template <class T>
class BatchRing {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    explicit BatchRing(std::size_t capacity)
        : cap_(llds::next_pow2(capacity)), mask_(cap_ - 1), buf_(cap_) {}

    std::size_t capacity() const noexcept { return cap_; }

    std::size_t push_batch(const T* items, std::size_t n) noexcept {
        // TODO
        (void)items;
        (void)n;
        return 0;
    }

    std::size_t pop_batch(T* out, std::size_t max) noexcept {
        // TODO
        (void)out;
        (void)max;
        return 0;
    }

private:
    const std::size_t cap_, mask_;
    std::vector<T> buf_;
    alignas(llds::kFalseSharingRange) std::atomic<std::uint64_t> head_{0};
    std::uint64_t tail_cache_ = 0;  // consumer-private
    alignas(llds::kFalseSharingRange) std::atomic<std::uint64_t> tail_{0};
    std::uint64_t head_cache_ = 0;  // producer-private
};
