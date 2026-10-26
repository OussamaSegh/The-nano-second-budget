// Exercise F8.1 (solution): the first SPSC ring, line by line.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

template <class T, std::size_t N>
class ModRing {
    static_assert(N >= 2, "one slot always stays empty, so N >= 2");
    static_assert(std::is_nothrow_default_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>);

public:
    static constexpr std::size_t capacity = N - 1;

    // Producer only.
    bool try_push(const T& v) noexcept {
        const auto w = write_.load(std::memory_order_relaxed);  // only we write it
        const auto next = (w + 1) % N;
        // acquire: pairs with the consumer's release of read_, so its copy out
        // of slot `next` is finished before we overwrite that slot (reuse edge).
        if (next == read_.load(std::memory_order_acquire)) return false;  // full
        slots_[w] = v;
        // release: the slot write above is visible before the new index
        // (publication edge).
        write_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer only. `out` is unchanged on failure.
    bool try_pop(T& out) noexcept {
        const auto r = read_.load(std::memory_order_relaxed);  // only we write it
        if (r == write_.load(std::memory_order_acquire)) return false;  // empty; pairs with push's release
        out = slots_[r];
        read_.store((r + 1) % N, std::memory_order_release);  // hand the slot back
        return true;
    }

private:
    std::array<T, N> slots_{};
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};
