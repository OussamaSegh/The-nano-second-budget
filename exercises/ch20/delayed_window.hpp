// Exercise 20.2: estimate randoms with a delayed window.
//
// Pair every hit with the hits that arrive about `delay` later, where the
// delay is far longer than any true coincidence's time difference. Only
// unrelated photons can pair that way, so the count estimates the number of
// random coincidences in the prompt window of the same width.
//
// Count pairs (i, j) of hits on different modules with
//     delay <= tick[j] - tick[i] <= delay + window,
// that is, the prompt sorter's forward window [0, window] (each hit pairs
// with later hits at most `window` after it) shifted by `delay`. Using the
// same window shape as the prompt sorter is what makes the two counts
// comparable: a window twice as wide would count twice as many randoms.
// Require delay > window (throw std::invalid_argument otherwise) and sorted
// input (throw if not). Aim for O(n + C), not O(n^2): two indices that only
// move forward. Do not let tick + delay + window overflow.
#pragma once

#include <llds/pet/hit.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>

inline std::uint64_t delayed_window_count(std::span<const llds::pet::Hit> hits, std::uint64_t window,
                                          std::uint64_t delay) {
    // TODO
    (void)hits;
    (void)window;
    (void)delay;
    return 0;
}
