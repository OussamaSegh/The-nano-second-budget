// Exercise 20.1: single-window coincidence grouping.
//
// The library's pairing (llds/pet/coincidence.hpp) is "multiple window":
// every hit opens its own window. Many scanners instead use a single window:
//
//   1. The earliest unassigned hit opens a window of width `window`.
//   2. Every following hit with tick - opening.tick <= window joins the group.
//   3. The next window opens at the first hit after the group.
//
// Classify each group: one hit is a single; two hits are a coincidence if
// their modules differ (else count same_module); three or more are a
// multiple. Count every multiple; with Multiples::take_all_pairs also emit
// all its cross-module pairs (ordered by later hit, then earlier hit, as
// window_pairs does), with Multiples::kill_all emit none.
//
// Throw std::invalid_argument if the input is not sorted by tick.
#pragma once

#include <llds/pet/coincidence.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

enum class Multiples { kill_all, take_all_pairs };

struct SingleWindowResult {
    std::vector<llds::pet::Coincidence> pairs;
    std::uint64_t singles = 0;
    std::uint64_t same_module = 0;
    std::uint64_t multiples = 0;
};

inline SingleWindowResult single_window(std::span<const llds::pet::Hit> hits, std::uint64_t window, Multiples policy) {
    // TODO
    (void)hits;
    (void)window;
    (void)policy;
    return {};
}
