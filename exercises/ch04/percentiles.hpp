// Exercise 4.1: implement nearest-rank percentiles on a sorted sample.
// Exercise 4.2: implement coordinated-omission correction.
//
// See Chapter 4 for the definitions. The tests pin down the exact
// conventions (nearest rank, p in (0, 100]).
#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <llds/common/histogram.hpp>

inline std::uint64_t percentile_sorted(const std::vector<std::uint64_t>& sorted, double p) {
    // TODO: nearest-rank. Throw std::invalid_argument on an empty sample.
    (void)sorted;
    (void)p;
    return 0;
}

inline void record_corrected(llds::Histogram& h, std::uint64_t value, std::uint64_t expected_interval) {
    // TODO: record `value`, plus the samples a constant-rate load generator
    // would have taken while this one was stuck: value - interval,
    // value - 2*interval, ... while they are >= interval.
    (void)expected_interval;
    h.record(value);
}
