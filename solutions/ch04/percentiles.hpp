// Exercise 4.1 and 4.2 (solutions).
#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <llds/common/histogram.hpp>

// 4.1: nearest-rank percentile of an already sorted sample.
// p in (0, 100]; p = 50 is the median, p = 100 the maximum.
inline std::uint64_t percentile_sorted(const std::vector<std::uint64_t>& sorted, double p) {
    if (sorted.empty()) throw std::invalid_argument("empty sample");
    if (p <= 0.0) return sorted.front();
    if (p >= 100.0) return sorted.back();
    // Nearest rank: the smallest value with at least p% of samples <= it.
    const auto rank = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(sorted.size())));
    return sorted[rank - 1];
}

// 4.2: record a sample from a closed-loop benchmark that *should* have
// issued a request every `expected_interval`. If the sample took longer,
// the requests that would have been sent meanwhile were delayed too:
// back-fill them with linearly decreasing latencies.
inline void record_corrected(llds::Histogram& h, std::uint64_t value, std::uint64_t expected_interval) {
    h.record(value);
    if (expected_interval == 0 || value <= expected_interval) return;
    for (std::uint64_t missing = value - expected_interval; missing >= expected_interval;
         missing -= expected_interval)
        h.record(missing);
}
