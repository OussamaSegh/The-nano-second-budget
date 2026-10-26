// Exercise F8.1: the first SPSC ring, line by line.
//
// N physical slots hold at most N - 1 items: write_ == read_ means empty,
// and (write_ + 1) % N == read_ means full, so one slot always stays free.
// N need not be a power of two. Exactly one thread calls try_push and
// exactly one calls try_pop.
//
// Implement both with the weakest correct memory orders and write, next to
// each atomic operation, which of the two edges it provides: publication
// (producer's slot write before consumer's slot read) or reuse (consumer's
// slot read before producer's next write to the same slot).
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

template <class T, std::size_t N>
class ModRing {
    static_assert(N >= 2);
    static_assert(std::is_nothrow_default_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>);

public:
    static constexpr std::size_t capacity = N - 1;

    bool try_push(const T& v) noexcept {
        // TODO
        (void)v;
        return false;
    }

    bool try_pop(T& out) noexcept {
        // TODO
        (void)out;
        return false;
    }

private:
    std::array<T, N> slots_{};
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};
