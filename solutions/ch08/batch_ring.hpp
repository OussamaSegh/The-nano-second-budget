// Exercise 8.1 (solution): an SPSC ring with batch operations.
//
// Batching amortises the one cross-core cost that remains in SpscRing: the
// release store of the index (and the cache-line transfer it triggers). A
// batch of k items costs one index publish instead of k.
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

    // Push up to n items; returns how many were pushed (possibly 0).
    std::size_t push_batch(const T* items, std::size_t n) noexcept {
        const auto t = tail_.load(std::memory_order_relaxed);
        auto free_slots = cap_ - (t - head_cache_);
        if (free_slots < n) {
            head_cache_ = head_.load(std::memory_order_acquire);
            free_slots = cap_ - (t - head_cache_);
        }
        const auto k = std::min(n, static_cast<std::size_t>(free_slots));
        for (std::size_t i = 0; i < k; ++i) buf_[(t + i) & mask_] = items[i];
        if (k) tail_.store(t + k, std::memory_order_release);  // one publish per batch
        return k;
    }

    // Pop up to max items into out; returns how many were popped.
    std::size_t pop_batch(T* out, std::size_t max) noexcept {
        const auto h = head_.load(std::memory_order_relaxed);
        auto avail = tail_cache_ - h;
        if (avail < max) {
            tail_cache_ = tail_.load(std::memory_order_acquire);
            avail = tail_cache_ - h;
        }
        const auto k = std::min(max, static_cast<std::size_t>(avail));
        for (std::size_t i = 0; i < k; ++i) out[i] = buf_[(h + i) & mask_];
        if (k) head_.store(h + k, std::memory_order_release);
        return k;
    }

private:
    const std::size_t cap_, mask_;
    std::vector<T> buf_;
    alignas(llds::kFalseSharingRange) std::atomic<std::uint64_t> head_{0};
    std::uint64_t tail_cache_ = 0;  // consumer-private
    alignas(llds::kFalseSharingRange) std::atomic<std::uint64_t> tail_{0};
    std::uint64_t head_cache_ = 0;  // producer-private
};
