// Exercise 20.2 (solution): estimate randoms with a delayed window.
#pragma once

#include <llds/pet/hit.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>

// Count pairs (i, j), modules differ, with
//   delay <= tick[j] - tick[i] <= delay + window:
// the prompt sorter's forward window [0, window], shifted by `delay`.
// Two pointers that only move forward: O(n + C).
inline std::uint64_t delayed_window_count(std::span<const llds::pet::Hit> hits, std::uint64_t window,
                                          std::uint64_t delay) {
    if (delay <= window) throw std::invalid_argument("delayed_window_count: delay must exceed the window");
    for (std::size_t j = 1; j < hits.size(); ++j)
        if (hits[j].tick < hits[j - 1].tick) throw std::invalid_argument("delayed_window_count: input not sorted");
    std::uint64_t count = 0;
    std::size_t lo = 0, hi = 0;  // candidate partners of hit i are [lo, hi)
    for (std::size_t i = 0; i < hits.size(); ++i) {
        const std::uint64_t t = hits[i].tick;
        // Written as subtractions from tick[j] (>= t once j > i) so that
        // t + delay + window cannot overflow near the top of the range.
        while (lo < hits.size() && (hits[lo].tick < t || hits[lo].tick - t < delay)) ++lo;
        if (hi < lo) hi = lo;
        while (hi < hits.size() && hits[hi].tick - t <= delay + window) ++hi;
        for (std::size_t j = lo; j < hi; ++j)
            if (hits[j].module != hits[i].module) ++count;
    }
    return count;
}
